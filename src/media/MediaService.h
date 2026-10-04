#pragma once

#include <QString>

class ProcessRunner;

class MediaService
{
public:
    MediaService(
        ProcessRunner& ffmpeg,
        ProcessRunner& ffprobe
    );

    bool createVideoFromImageAndAudio(
        const QString& imagePath,
        const QString& audioPath,
        const QString& outputPath,
        QString* errorMessage = nullptr
    );

private:
    bool getMediaDuration(
        const QString& filePath,
        double& duration,
        QString* errorMessage
    );

    ProcessRunner& m_ffmpeg;
    ProcessRunner& m_ffprobe;
};