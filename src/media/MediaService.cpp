#include "MediaService.h"

#include "ProcessRunner.h"

#include <algorithm>
#include <QFile>
#include <QThread>

MediaService::MediaService(
    ProcessRunner& ffmpeg,
    ProcessRunner& ffprobe,
    QObject* parent
)
    : QObject(parent),
      m_ffmpeg(ffmpeg),
      m_ffprobe(ffprobe)
{
    connect(&m_ffmpeg, &ProcessRunner::finished,
            this, &MediaService::onFfmpegFinished);

    connect(&m_ffmpeg, &ProcessRunner::errorOccurred,
            this, &MediaService::onFfmpegError);

    connect(&m_ffmpeg, &ProcessRunner::standardOutputLineReceived,
            this, &MediaService::onStandardOutputLineReceived);
}

bool MediaService::createVideoFromImageAndAudio(
    const QString& imagePath,
    const QString& audioPath,
    const QString& outputPath,
    QString* errorMessage)
{
    if (m_isProcessing) {
        if (errorMessage) {
            *errorMessage = "A media job is already in progress.";
        }
        return false;
    }

    double audioDuration = 0.0;
    if (!getMediaDuration(audioPath, audioDuration, errorMessage)) {
        return false;
    }

    const QStringList arguments = {
        "-y",

        "-progress", "pipe:1",
        "-nostats",

        "-loop", "1",
        "-i", imagePath,

        "-i", audioPath,

        "-vf", "pad=ceil(iw/2)*2:ceil(ih/2)*2,format=yuv420p",

        "-c:v", "libx264",
        "-r", "5",
        "-pix_fmt", "yuv420p",
        "-color_range", "tv",

        "-c:a", "aac",
        "-b:a", "128k",

        "-t", QString::number(audioDuration, 'f', 6),

        outputPath
    };

    m_isProcessing = true;
    m_isCancelling = false;
    m_audioDuration = audioDuration;
    m_currentOutputPath = outputPath;

    if (!m_ffmpeg.start(arguments)) {
        m_isProcessing = false;
        m_audioDuration = 0.0;
        if (errorMessage) {
            *errorMessage = "Failed to start FFmpeg process.";
        }
        return false;
    }

    emit videoCreationStarted();
    emit progressChanged(0);
    return true;
}

bool MediaService::isProcessing() const
{
    return m_isProcessing;
}

void MediaService::cancel()
{
    if (m_isProcessing && !m_isCancelling) {
        m_isCancelling = true;
        m_ffmpeg.cancel();
    }
}

void MediaService::onStandardOutputLineReceived(const QString& line)
{
    if (!m_isProcessing || m_audioDuration <= 0.0) {
        return;
    }

    if (line.startsWith(QLatin1String("out_time_us="))) {
        bool ok = false;
        const qint64 outTimeUs = QStringView(line).mid(12).trimmed().toLongLong(&ok);
        if (ok && outTimeUs >= 0) {
            const double totalUs = m_audioDuration * 1000000.0;
            int percentage = static_cast<int>((static_cast<double>(outTimeUs) / totalUs) * 100.0);
            percentage = std::clamp(percentage, 0, 100);
            emit progressChanged(percentage);
        }
    } else if (line.startsWith(QLatin1String("progress=end"))) {
        emit progressChanged(100);
    }
}

void MediaService::onFfmpegFinished(
    int exitCode,
    QProcess::ExitStatus exitStatus,
    const QString& stdoutStr,
    const QString& stderrStr)
{
    Q_UNUSED(stdoutStr);

    if (!m_isProcessing) {
        return;
    }

    if (m_isCancelling) {
        m_isProcessing = false;
        m_isCancelling = false;
        m_audioDuration = 0.0;

        // Clean up incomplete/corrupted output file with retry (allowing OS handle release)
        if (!m_currentOutputPath.isEmpty() && QFile::exists(m_currentOutputPath)) {
            for (int attempt = 0; attempt < 10; ++attempt) {
                if (QFile::remove(m_currentOutputPath)) {
                    break;
                }
                QThread::msleep(30);
            }
        }

        emit videoCreationCancelled();
        return;
    }

    m_isProcessing = false;
    m_audioDuration = 0.0;

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        emit progressChanged(100);
        emit videoCreationFinished(true, QString());
    } else {
        QString errorMsg = stderrStr.trimmed();
        if (errorMsg.isEmpty()) {
            errorMsg = QString("FFmpeg process failed with exit code %1.").arg(exitCode);
        }
        emit videoCreationFinished(false, errorMsg);
    }
}

void MediaService::onFfmpegError(const QString& errorMessage)
{
    if (!m_isProcessing) {
        return;
    }

    if (m_isCancelling) {
        m_isProcessing = false;
        m_isCancelling = false;
        m_audioDuration = 0.0;

        if (!m_currentOutputPath.isEmpty() && QFile::exists(m_currentOutputPath)) {
            for (int attempt = 0; attempt < 10; ++attempt) {
                if (QFile::remove(m_currentOutputPath)) {
                    break;
                }
                QThread::msleep(30);
            }
        }

        emit videoCreationCancelled();
        return;
    }

    m_isProcessing = false;
    m_audioDuration = 0.0;
    emit videoCreationFinished(false, errorMessage);
}

bool MediaService::getMediaDuration(
    const QString& filePath,
    double& duration,
    QString* errorMessage)
{
    QString output;
    QString error;

    const QStringList arguments = {
        "-v", "error",
        "-show_entries", "format=duration",
        "-of", "default=noprint_wrappers=1:nokey=1",
        filePath
    };

    if (!m_ffprobe.run(arguments, &output, &error)) {
        if (errorMessage) {
            *errorMessage = error;
        }

        return false;
    }

    bool ok = false;
    const double parsedDuration = output.trimmed().toDouble(&ok);

    if (!ok || parsedDuration <= 0.0) {
        if (errorMessage) {
            *errorMessage =
                "Could not determine media duration.";
        }

        return false;
    }

    duration = parsedDuration;
    return true;
}
