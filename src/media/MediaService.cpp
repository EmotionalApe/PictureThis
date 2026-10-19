#include "MediaService.h"
#include "ProcessRunner.h"

#include <algorithm>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
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
    m_currentJob = MediaJobType::ImageToVideo;
    m_targetDuration = audioDuration;
    m_currentOutputPath = outputPath;

    if (!m_ffmpeg.start(arguments)) {
        m_isProcessing = false;
        m_currentJob = MediaJobType::None;
        m_targetDuration = 0.0;
        if (errorMessage) {
            *errorMessage = "Failed to start FFmpeg process.";
        }
        return false;
    }

    emit videoCreationStarted();
    emit progressChanged(0);
    return true;
}

bool MediaService::trimVideo(
    const QString& inputPath,
    double startTime,
    double endTime,
    const QString& outputPath,
    bool accurateReencode,
    QString* errorMessage)
{
    if (m_isProcessing) {
        if (errorMessage) {
            *errorMessage = "A media job is already in progress.";
        }
        return false;
    }

    if (startTime < 0.0 || endTime <= startTime) {
        if (errorMessage) {
            *errorMessage = "Invalid start or end timestamp for trimming.";
        }
        return false;
    }

    QStringList arguments;
    arguments << "-y" << "-progress" << "pipe:1" << "-nostats";

    if (!accurateReencode) {
        // Fast stream copy mode
        arguments << "-ss" << QString::number(startTime, 'f', 6)
                  << "-to" << QString::number(endTime, 'f', 6)
                  << "-i" << inputPath
                  << "-c" << "copy"
                  << "-avoid_negative_ts" << "make_zero"
                  << outputPath;
    } else {
        // Frame-accurate re-encode mode
        arguments << "-ss" << QString::number(startTime, 'f', 6)
                  << "-to" << QString::number(endTime, 'f', 6)
                  << "-i" << inputPath
                  << "-c:v" << "libx264"
                  << "-crf" << "20"
                  << "-c:a" << "aac"
                  << "-b:a" << "128k"
                  << outputPath;
    }

    m_isProcessing = true;
    m_isCancelling = false;
    m_currentJob = MediaJobType::VideoTrim;
    m_targetDuration = std::max(0.001, endTime - startTime);
    m_currentOutputPath = outputPath;

    if (!m_ffmpeg.start(arguments)) {
        m_isProcessing = false;
        m_currentJob = MediaJobType::None;
        m_targetDuration = 0.0;
        if (errorMessage) {
            *errorMessage = "Failed to start FFmpeg process.";
        }
        return false;
    }

    emit videoTrimStarted();
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
    if (!m_isProcessing || m_targetDuration <= 0.0) {
        return;
    }

    if (line.startsWith(QLatin1String("out_time_us="))) {
        bool ok = false;
        const qint64 outTimeUs = QStringView(line).mid(12).trimmed().toLongLong(&ok);
        if (ok && outTimeUs >= 0) {
            const double totalUs = m_targetDuration * 1000000.0;
            int percentage = static_cast<int>((static_cast<double>(outTimeUs) / totalUs) * 100.0);
            percentage = std::clamp(percentage, 0, 100);
            emit progressChanged(percentage);
        }
    } else if (line.startsWith(QLatin1String("progress=end"))) {
        emit progressChanged(100);
    }
}

void MediaService::handleJobCompletion(bool success, const QString& errorMessage)
{
    const MediaJobType job = m_currentJob;
    m_isProcessing = false;
    m_currentJob = MediaJobType::None;
    m_targetDuration = 0.0;

    if (success) {
        emit progressChanged(100);
    }

    if (job == MediaJobType::ImageToVideo) {
        emit videoCreationFinished(success, errorMessage);
    } else if (job == MediaJobType::VideoTrim) {
        emit videoTrimFinished(success, errorMessage);
    }
}

void MediaService::handleJobCancellation()
{
    const MediaJobType job = m_currentJob;
    m_isProcessing = false;
    m_isCancelling = false;
    m_currentJob = MediaJobType::None;
    m_targetDuration = 0.0;

    // Clean up incomplete/corrupted output file with retry
    if (!m_currentOutputPath.isEmpty() && QFile::exists(m_currentOutputPath)) {
        for (int attempt = 0; attempt < 10; ++attempt) {
            if (QFile::remove(m_currentOutputPath)) {
                break;
            }
            QThread::msleep(30);
        }
    }

    emit progressChanged(0);

    if (job == MediaJobType::ImageToVideo) {
        emit videoCreationCancelled();
    } else if (job == MediaJobType::VideoTrim) {
        emit videoTrimCancelled();
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
        handleJobCancellation();
        return;
    }

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        handleJobCompletion(true, QString());
    } else {
        QString errorMsg = stderrStr.trimmed();
        if (errorMsg.isEmpty()) {
            errorMsg = QString("FFmpeg process failed with exit code %1.").arg(exitCode);
        }
        handleJobCompletion(false, errorMsg);
    }
}

void MediaService::onFfmpegError(const QString& errorMessage)
{
    if (!m_isProcessing) {
        return;
    }

    if (m_isCancelling) {
        handleJobCancellation();
        return;
    }

    handleJobCompletion(false, errorMessage);
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
            *errorMessage = "Could not determine media duration.";
        }
        return false;
    }

    duration = parsedDuration;
    return true;
}

bool MediaService::getVideoMetadata(
    const QString& filePath,
    VideoMetadata& outMeta,
    QString* errorMessage)
{
    QString output;
    QString error;

    const QStringList arguments = {
        "-v", "error",
        "-show_entries", "stream=codec_type,codec_name,width,height:format=duration,size",
        "-of", "json",
        filePath
    };

    if (!m_ffprobe.run(arguments, &output, &error)) {
        if (errorMessage) {
            *errorMessage = error.isEmpty() ? "Failed to probe video file." : error;
        }
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
    if (!doc.isObject()) {
        if (errorMessage) {
            *errorMessage = "Invalid metadata format returned by ffprobe.";
        }
        return false;
    }

    const QJsonObject rootObj = doc.object();

    // Parse streams
    const QJsonArray streams = rootObj.value("streams").toArray();
    for (const QJsonValue& val : streams) {
        const QJsonObject streamObj = val.toObject();
        const QString type = streamObj.value("codec_type").toString();
        const QString codec = streamObj.value("codec_name").toString();

        if (type == "video" && outMeta.videoCodec.isEmpty()) {
            outMeta.videoCodec = codec;
            outMeta.width = streamObj.value("width").toInt();
            outMeta.height = streamObj.value("height").toInt();
        } else if (type == "audio" && outMeta.audioCodec.isEmpty()) {
            outMeta.audioCodec = codec;
        }
    }

    // Parse format (duration & size)
    const QJsonObject formatObj = rootObj.value("format").toObject();
    outMeta.durationSeconds = formatObj.value("duration").toString().toDouble();
    outMeta.fileSizeBytes = formatObj.value("size").toString().toLongLong();

    if (outMeta.fileSizeBytes <= 0) {
        outMeta.fileSizeBytes = QFileInfo(filePath).size();
    }

    if (outMeta.durationSeconds <= 0.0) {
        // Fallback: try getMediaDuration
        double dur = 0.0;
        if (getMediaDuration(filePath, dur)) {
            outMeta.durationSeconds = dur;
        }
    }

    return true;
}

bool MediaService::extractThumbnail(
    const QString& filePath,
    double timestampSeconds,
    QPixmap& outPixmap,
    QString* errorMessage)
{
    QByteArray imageData;
    QString stdErr;

    const QStringList arguments = {
        "-ss", QString::number(std::max(0.0, timestampSeconds), 'f', 3),
        "-i", filePath,
        "-vframes", "1",
        "-f", "image2pipe",
        "-vcodec", "mjpeg",
        "-"
    };

    if (!m_ffmpeg.runBinary(arguments, &imageData, &stdErr)) {
        if (errorMessage) {
            *errorMessage = stdErr.isEmpty() ? "Failed to extract thumbnail." : stdErr;
        }
        return false;
    }

    if (!outPixmap.loadFromData(imageData, "JPG")) {
        if (errorMessage) {
            *errorMessage = "Failed to load thumbnail image data.";
        }
        return false;
    }

    return true;
}
