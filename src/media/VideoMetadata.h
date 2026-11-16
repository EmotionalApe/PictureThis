#pragma once

#include <QString>
#include <cstdint>

struct VideoMetadata {
    double durationSeconds = 0.0;
    int width = 0;
    int height = 0;
    QString videoCodec;
    QString audioCodec;
    qint64 fileSizeBytes = 0;

    // Container & format parameters
    QString formatName;
    QString formatLongName;
    qint64 overallBitrate = 0;

    // Video stream parameters
    QString videoCodecLongName;
    QString videoProfile;
    QString videoLevel;
    QString pixelFormat;
    QString colorRange;
    QString colorSpace;
    QString colorTransfer;
    QString colorPrimaries;
    QString displayAspectRatio;
    QString sampleAspectRatio;
    double frameRate = 0.0;
    qint64 videoBitrate = 0;

    // Audio stream parameters
    bool hasAudio = false;
    QString audioCodecLongName;
    int audioSampleRate = 0;
    int audioChannels = 0;
    QString audioChannelLayout;
    qint64 audioBitrate = 0;
};
