#pragma once

#include <QString>
#include <QPixmap>
#include <cstdint>
#include "MediaService.h"

enum class InspectionKind {
    Metadata,
    Thumbnail
};

enum class InspectionStatus {
    Queued,
    Inspecting,
    Ready,
    Failed,
    Cancelled
};

struct RequestId {
    uint64_t serial = 0;
    QString filePath;
    double timestampSeconds = 0.0; // relevant for thumbnails
    QString purpose;               // e.g. "trim_start", "trim_end", "combine_queue"

    bool isValid() const { return serial > 0; }
    bool operator==(const RequestId& other) const {
        return serial == other.serial;
    }
    bool operator!=(const RequestId& other) const {
        return serial != other.serial;
    }
};

struct InspectionTask {
    RequestId id;
    InspectionKind kind = InspectionKind::Metadata;
    QString filePath;
    double timestampSeconds = 0.0;
    int targetWidth = 0;  // 0 for default/original aspect
    int targetHeight = 0;
};

struct ProbeResult {
    RequestId id;
    bool success = false;
    VideoMetadata metadata;
    QString errorMessage;
};

struct ThumbnailResult {
    RequestId id;
    bool success = false;
    QPixmap thumbnail;
    QString errorMessage;
};
