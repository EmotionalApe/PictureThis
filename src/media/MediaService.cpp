#include "MediaService.h"
#include "ProcessRunner.h"

#include <algorithm>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTextStream>
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

bool MediaService::combineVideos(
    const QStringList& inputPaths,
    const QString& outputPath,
    bool forceReencode,
    QString* errorMessage)
{
    if (m_isProcessing) {
        if (errorMessage) {
            *errorMessage = "A media job is already in progress.";
        }
        return false;
    }

    if (inputPaths.size() < 2) {
        if (errorMessage) {
            *errorMessage = "At least two video files are required to combine.";
        }
        return false;
    }

    for (const QString& path : inputPaths) {
        if (!QFile::exists(path)) {
            if (errorMessage) {
                *errorMessage = QString("Input file does not exist: %1").arg(path);
            }
            return false;
        }
    }

    // Probe metadata for all inputs
    QList<VideoMetadata> metadatas;
    metadatas.reserve(inputPaths.size());
    double totalDuration = 0.0;

    int maxArea = 0;
    int targetWidth = 1920;
    int targetHeight = 1080;

    bool allMatching = true;
    VideoMetadata firstMeta;

    for (int i = 0; i < inputPaths.size(); ++i) {
        VideoMetadata meta;
        if (!getVideoMetadata(inputPaths[i], meta, errorMessage)) {
            return false;
        }
        metadatas.append(meta);
        totalDuration += meta.durationSeconds;

        const int area = meta.width * meta.height;
        if (area > maxArea) {
            maxArea = area;
            targetWidth = meta.width;
            targetHeight = meta.height;
        }

        if (i == 0) {
            firstMeta = meta;
        } else {
            if (meta.width != firstMeta.width ||
                meta.height != firstMeta.height ||
                meta.videoCodec != firstMeta.videoCodec ||
                meta.audioCodec != firstMeta.audioCodec) {
                allMatching = false;
            }
        }
    }

    // Ensure even canvas dimensions for H.264
    targetWidth = (targetWidth + 1) & ~1;
    targetHeight = (targetHeight + 1) & ~1;

    QStringList arguments;
    arguments << "-y" << "-progress" << "pipe:1" << "-nostats";

    cleanupTemporaryFiles();

    if (allMatching && !forceReencode) {
        // Mode 1: Concat Demuxer (Fast Lossless Stream Copy)
        const QString manifestName = QString("concat_%1_%2.txt")
            .arg(QDateTime::currentMSecsSinceEpoch())
            .arg(QCoreApplication::applicationPid());
        const QString manifestPath = QDir::temp().filePath(manifestName);

        QFile manifestFile(manifestPath);
        if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (errorMessage) {
                *errorMessage = "Failed to create temporary concat manifest file.";
            }
            return false;
        }

        QTextStream outStream(&manifestFile);
        for (const QString& path : inputPaths) {
            QString escapedPath = QDir::fromNativeSeparators(path);
            escapedPath.replace("'", "'\\''");
            outStream << "file '" << escapedPath << "'\n";
        }
        manifestFile.close();

        m_concatManifestPath = manifestPath;

        arguments << "-f" << "concat"
                  << "-safe" << "0"
                  << "-i" << manifestPath
                  << "-c" << "copy"
                  << outputPath;
    } else {
        // Mode 2: Concat Filter Complex (Smart Re-encode Fallback)
        for (const QString& path : inputPaths) {
            arguments << "-i" << path;
        }

        QString filterGraph;
        QString concatInputs;

        for (int i = 0; i < inputPaths.size(); ++i) {
            // Video stream normalization: scale to fit within canvas preserving aspect ratio, center pad
            filterGraph += QString("[%1:v]scale=%2:%3:force_original_aspect_ratio=decrease,pad=%2:%3:(ow-iw)/2:(oh-ih)/2,setsar=1[v%1];")
                .arg(i).arg(targetWidth).arg(targetHeight);

            // Audio handling: normalize audio or generate silence if video has no audio
            if (!metadatas[i].audioCodec.isEmpty()) {
                filterGraph += QString("[%1:a]aformat=sample_fmts=fltp:sample_rates=44100:channel_layouts=stereo[a%1];")
                    .arg(i);
            } else {
                const double dur = std::max(0.1, metadatas[i].durationSeconds);
                filterGraph += QString("aevalsrc=0:d=%1:s=44100[a%1];")
                    .arg(dur, 0, 'f', 3).arg(i);
            }

            concatInputs += QString("[v%1][a%1]").arg(i);
        }

        filterGraph += QString("%1concat=n=%2:v=1:a=1[outv][outa]")
            .arg(concatInputs)
            .arg(inputPaths.size());

        arguments << "-filter_complex" << filterGraph
                  << "-map" << "[outv]"
                  << "-map" << "[outa]"
                  << "-c:v" << "libx264"
                  << "-crf" << "20"
                  << "-pix_fmt" << "yuv420p"
                  << "-color_range" << "tv"
                  << "-c:a" << "aac"
                  << "-b:a" << "128k"
                  << outputPath;
    }

    m_isProcessing = true;
    m_isCancelling = false;
    m_currentJob = MediaJobType::VideoCombine;
    m_targetDuration = std::max(0.001, totalDuration);
    m_currentOutputPath = outputPath;

    if (!m_ffmpeg.start(arguments)) {
        m_isProcessing = false;
        m_currentJob = MediaJobType::None;
        m_targetDuration = 0.0;
        cleanupTemporaryFiles();
        if (errorMessage) {
            *errorMessage = "Failed to start FFmpeg process.";
        }
        return false;
    }

    emit videoCombineStarted();
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

void MediaService::cleanupTemporaryFiles()
{
    if (!m_concatManifestPath.isEmpty()) {
        if (QFile::exists(m_concatManifestPath)) {
            QFile::remove(m_concatManifestPath);
        }
        m_concatManifestPath.clear();
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

    cleanupTemporaryFiles();

    if (success) {
        emit progressChanged(100);
    }

    if (job == MediaJobType::ImageToVideo) {
        emit videoCreationFinished(success, errorMessage);
    } else if (job == MediaJobType::VideoTrim) {
        emit videoTrimFinished(success, errorMessage);
    } else if (job == MediaJobType::VideoCombine) {
        emit videoCombineFinished(success, errorMessage);
    }
}

void MediaService::handleJobCancellation()
{
    const MediaJobType job = m_currentJob;
    m_isProcessing = false;
    m_isCancelling = false;
    m_currentJob = MediaJobType::None;
    m_targetDuration = 0.0;

    cleanupTemporaryFiles();

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
    } else if (job == MediaJobType::VideoCombine) {
        emit videoCombineCancelled();
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
