#include "InspectionScheduler.h"
#include "InspectionWorker.h"

InspectionScheduler::InspectionScheduler(
    QString ffmpegExe,
    QString ffprobeExe,
    int maxConcurrent,
    QObject* parent)
    : QObject(parent),
      m_ffmpegExe(std::move(ffmpegExe)),
      m_ffprobeExe(std::move(ffprobeExe)),
      m_maxConcurrent(std::max(1, maxConcurrent))
{
    for (int i = 0; i < m_maxConcurrent; ++i) {
        auto* worker = new InspectionWorker(i + 1, this);
        connect(worker, &InspectionWorker::metadataFinished,
                this, &InspectionScheduler::onWorkerMetadataFinished);
        connect(worker, &InspectionWorker::thumbnailFinished,
                this, &InspectionScheduler::onWorkerThumbnailFinished);
        connect(worker, &InspectionWorker::workerFinished,
                this, &InspectionScheduler::onWorkerFinished);
        m_workers.append(worker);
    }
}

InspectionScheduler::~InspectionScheduler()
{
    cancelAll();
}

RequestId InspectionScheduler::generateRequestId(const QString& filePath, double ts, const QString& purpose)
{
    RequestId req;
    req.serial = m_nextSerial++;
    req.filePath = filePath;
    req.timestampSeconds = ts;
    req.purpose = purpose;
    return req;
}

RequestId InspectionScheduler::requestMetadata(
    const QString& filePath,
    const QString& purpose,
    bool highPriority)
{
    const RequestId req = generateRequestId(filePath, 0.0, purpose);

    InspectionTask task;
    task.id = req;
    task.kind = InspectionKind::Metadata;
    task.filePath = filePath;

    if (highPriority) {
        m_pendingQueue.prepend(task);
    } else {
        m_pendingQueue.append(task);
    }

    emit inspectionStatusChanged(req, InspectionStatus::Queued);
    dispatchNextTasks();
    return req;
}

RequestId InspectionScheduler::requestThumbnail(
    const QString& filePath,
    double timestampSeconds,
    int targetWidth,
    int targetHeight,
    const QString& purpose,
    bool highPriority)
{
    const RequestId req = generateRequestId(filePath, timestampSeconds, purpose);

    InspectionTask task;
    task.id = req;
    task.kind = InspectionKind::Thumbnail;
    task.filePath = filePath;
    task.timestampSeconds = timestampSeconds;
    task.targetWidth = targetWidth;
    task.targetHeight = targetHeight;

    if (highPriority) {
        m_pendingQueue.prepend(task);
    } else {
        m_pendingQueue.append(task);
    }

    emit inspectionStatusChanged(req, InspectionStatus::Queued);
    dispatchNextTasks();
    return req;
}

void InspectionScheduler::cancelRequest(const RequestId& id)
{
    if (!id.isValid()) {
        return;
    }

    m_cancelledSerials.insert(id.serial);

    // Remove from pending queue
    for (auto it = m_pendingQueue.begin(); it != m_pendingQueue.end(); ) {
        if (it->id.serial == id.serial) {
            it = m_pendingQueue.erase(it);
        } else {
            ++it;
        }
    }

    // Cancel on any active worker
    for (auto* worker : m_workers) {
        if (worker->isBusy() && worker->currentTask().id.serial == id.serial) {
            worker->cancel();
        }
    }

    emit inspectionStatusChanged(id, InspectionStatus::Cancelled);
    emit queueStatsChanged(activeCount(), queuedCount());
}

void InspectionScheduler::cancelRequestsForFile(const QString& filePath)
{
    if (filePath.isEmpty()) {
        return;
    }

    for (auto it = m_pendingQueue.begin(); it != m_pendingQueue.end(); ) {
        if (it->filePath == filePath) {
            m_cancelledSerials.insert(it->id.serial);
            emit inspectionStatusChanged(it->id, InspectionStatus::Cancelled);
            it = m_pendingQueue.erase(it);
        } else {
            ++it;
        }
    }

    for (auto* worker : m_workers) {
        if (worker->isBusy() && worker->currentTask().filePath == filePath) {
            m_cancelledSerials.insert(worker->currentTask().id.serial);
            worker->cancel();
        }
    }

    emit queueStatsChanged(activeCount(), queuedCount());
}

void InspectionScheduler::cancelRequestsForPurpose(const QString& purpose)
{
    if (purpose.isEmpty()) {
        return;
    }

    for (auto it = m_pendingQueue.begin(); it != m_pendingQueue.end(); ) {
        if (it->id.purpose == purpose) {
            m_cancelledSerials.insert(it->id.serial);
            emit inspectionStatusChanged(it->id, InspectionStatus::Cancelled);
            it = m_pendingQueue.erase(it);
        } else {
            ++it;
        }
    }

    for (auto* worker : m_workers) {
        if (worker->isBusy() && worker->currentTask().id.purpose == purpose) {
            m_cancelledSerials.insert(worker->currentTask().id.serial);
            worker->cancel();
        }
    }

    emit queueStatsChanged(activeCount(), queuedCount());
}

void InspectionScheduler::cancelAll()
{
    for (const auto& task : m_pendingQueue) {
        m_cancelledSerials.insert(task.id.serial);
        emit inspectionStatusChanged(task.id, InspectionStatus::Cancelled);
    }
    m_pendingQueue.clear();

    for (auto* worker : m_workers) {
        if (worker->isBusy()) {
            m_cancelledSerials.insert(worker->currentTask().id.serial);
            worker->cancel();
        }
    }

    emit queueStatsChanged(activeCount(), queuedCount());
}

bool InspectionScheduler::isInspecting() const
{
    return activeCount() > 0 || queuedCount() > 0;
}

int InspectionScheduler::activeCount() const
{
    int count = 0;
    for (const auto* worker : m_workers) {
        if (worker->isBusy()) {
            ++count;
        }
    }
    return count;
}

int InspectionScheduler::queuedCount() const
{
    return m_pendingQueue.size();
}

void InspectionScheduler::setFfmpegExecutable(const QString& path)
{
    m_ffmpegExe = path;
}

void InspectionScheduler::setFfprobeExecutable(const QString& path)
{
    m_ffprobeExe = path;
}

QString InspectionScheduler::ffmpegExecutable() const
{
    return m_ffmpegExe;
}

QString InspectionScheduler::ffprobeExecutable() const
{
    return m_ffprobeExe;
}

InspectionWorker* InspectionScheduler::getFreeWorker()
{
    for (auto* worker : m_workers) {
        if (!worker->isBusy()) {
            return worker;
        }
    }
    return nullptr;
}

void InspectionScheduler::dispatchNextTasks()
{
    while (!m_pendingQueue.isEmpty()) {
        InspectionWorker* worker = getFreeWorker();
        if (!worker) {
            break;
        }

        InspectionTask task = m_pendingQueue.takeFirst();

        // Check if cancelled before starting
        if (m_cancelledSerials.contains(task.id.serial)) {
            m_cancelledSerials.remove(task.id.serial);
            continue;
        }

        const QString& exe = (task.kind == InspectionKind::Metadata) ? m_ffprobeExe : m_ffmpegExe;
        if (!worker->startTask(task, exe)) {
            m_pendingQueue.prepend(task);
            break;
        }

        emit inspectionStatusChanged(task.id, InspectionStatus::Inspecting);
    }

    emit queueStatsChanged(activeCount(), queuedCount());
}

void InspectionScheduler::onWorkerMetadataFinished(const ProbeResult& result)
{
    if (m_cancelledSerials.contains(result.id.serial)) {
        // Discard stale or cancelled result
        m_cancelledSerials.remove(result.id.serial);
        return;
    }

    emit metadataReady(result);
    emit inspectionStatusChanged(result.id, result.success ? InspectionStatus::Ready : InspectionStatus::Failed);
}

void InspectionScheduler::onWorkerThumbnailFinished(const ThumbnailResult& result)
{
    if (m_cancelledSerials.contains(result.id.serial)) {
        // Discard stale or cancelled result
        m_cancelledSerials.remove(result.id.serial);
        return;
    }

    emit thumbnailReady(result);
    emit inspectionStatusChanged(result.id, result.success ? InspectionStatus::Ready : InspectionStatus::Failed);
}

void InspectionScheduler::onWorkerFinished(int workerId)
{
    Q_UNUSED(workerId);
    dispatchNextTasks();
}
