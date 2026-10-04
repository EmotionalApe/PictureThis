#include "MediaService.h"

#include "ProcessRunner.h"

#include <QRegularExpression>

MediaService::MediaService(
    ProcessRunner& ffmpeg,
    ProcessRunner& ffprobe
)
    : m_ffmpeg(ffmpeg),
    m_ffprobe(ffprobe)
{
}

bool MediaService::createVideoFromImageAndAudio(
    const QString& imagePath,
    const QString& audioPath,
    const QString& outputPath,
    QString* errorMessage)
{
    double audioDuration = 0.0;

    if (!getMediaDuration(
            audioPath,
            audioDuration,
            errorMessage)) {
        return false;
    }

    const QStringList arguments = {
        "-y",

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

    QString error;

    if (!m_ffmpeg.run(arguments, nullptr, &error)) {
        if (errorMessage) {
            *errorMessage = error;
        }

        return false;
    }

    return true;
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
