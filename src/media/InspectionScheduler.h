#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QSet>
#include <memory>
#include "InspectionTypes.h"
#include "MediaCache.h"

class InspectionWorker;

class InspectionScheduler : public QObject
{
    Q_OBJECT

public:
    explicit InspectionScheduler(
        QString ffmpegExe,
        QString ffprobeExe,
        int maxConcurrent = 2,
        QObject* parent = nullptr
    );
    ~InspectionScheduler() override;

    InspectionScheduler(const InspectionScheduler&) = delete;
    InspectionScheduler& operator=(const InspectionScheduler&) = delete;

    RequestId requestMetadata(
        const QString& filePath,
        const QString& purpose = {},
        bool highPriority = false
    );

    RequestId requestThumbnail(
        const QString& filePath,
        double timestampSeconds,
        int targetWidth = 0,
        int targetHeight = 0,
        const QString& purpose = {},
        bool highPriority = false
    );

    void cancelRequest(const RequestId& id);
    void cancelRequestsForFile(const QString& filePath);
    void cancelRequestsForPurpose(const QString& purpose);
    void cancelAll();

    bool isInspecting() const;
    int activeCount() const;
    int queuedCount() const;

    void setFfmpegExecutable(const QString& path);
    void setFfprobeExecutable(const QString& path);
    QString ffmpegExecutable() const;
    QString ffprobeExecutable() const;

signals:
    void metadataReady(const ProbeResult& result);
    void thumbnailReady(const ThumbnailResult& result);
    void inspectionStatusChanged(const RequestId& id, InspectionStatus status);
    void queueStatsChanged(int activeCount, int queuedCount);

private slots:
    void onWorkerMetadataFinished(const ProbeResult& result);
    void onWorkerThumbnailFinished(const ThumbnailResult& result);
    void onWorkerFinished(int workerId);

private:
    RequestId generateRequestId(const QString& filePath, double ts, const QString& purpose);
    void dispatchNextTasks();
    InspectionWorker* getFreeWorker();

    QString m_ffmpegExe;
    QString m_ffprobeExe;
    int m_maxConcurrent = 2;
    uint64_t m_nextSerial = 1;

    QList<InspectionWorker*> m_workers;
    QList<InspectionTask> m_pendingQueue;
    QSet<uint64_t> m_cancelledSerials;
    MediaCache m_cache;
};
