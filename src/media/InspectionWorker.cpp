#include "InspectionWorker.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

InspectionWorker::InspectionWorker(int workerId, QObject* parent)
    : QObject(parent),
      m_workerId(workerId),
      m_process(this)
{
    m_timeoutTimer.setSingleShot(true);
    m_timeoutTimer.setInterval(15000); // 15 seconds watchdog

    connect(&m_timeoutTimer, &QTimer::timeout, this, &InspectionWorker::onTimeout);

    connect(&m_process, &QProcess::readyReadStandardOutput,
            this, &InspectionWorker::onProcessReadyReadStandardOutput);
    connect(&m_process, &QProcess::readyReadStandardError,
            this, &InspectionWorker::onProcessReadyReadStandardError);
    connect(&m_process, &QProcess::finished,
            this, &InspectionWorker::onProcessFinished);
    connect(&m_process, &QProcess::errorOccurred,
            this, &InspectionWorker::onProcessError);
}

InspectionWorker::~InspectionWorker()
{
    m_timeoutTimer.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(500);
    }
}

bool InspectionWorker::isBusy() const
{
    return m_isBusy;
}

int InspectionWorker::workerId() const
{
    return m_workerId;
}

const InspectionTask& InspectionWorker::currentTask() const
{
    return m_currentTask;
}

bool InspectionWorker::startTask(const InspectionTask& task, const QString& executablePath)
{
    if (m_isBusy) {
        return false;
    }

    if (executablePath.isEmpty()) {
        m_currentTask = task;
        handleTaskCompletion(false, "Executable path is not configured.");
        return true;
    }

    m_currentTask = task;
    m_isBusy = true;
    m_isCancelled = false;
    m_outputBuffer.clear();
    m_errorBuffer.clear();

    QStringList arguments;

    if (task.kind == InspectionKind::Metadata) {
        arguments << "-v" << "error"
                  << "-show_entries" << "stream=codec_type,codec_name,width,height,duration:format=duration,size"
                  << "-of" << "json"
                  << task.filePath;
    } else {
        // Thumbnail extraction (high quality JPEG output)
        const double seekSeconds = std::max(0.0, task.timestampSeconds);
        arguments << "-ss" << QString::number(seekSeconds, 'f', 3)
                  << "-i" << task.filePath
                  << "-vframes" << "1"
                  << "-q:v" << "2"
                  << "-f" << "image2pipe"
                  << "-vcodec" << "mjpeg"
                  << "-";
    }

    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start(executablePath, arguments);
    m_timeoutTimer.start();
    return true;
}

void InspectionWorker::cancel()
{
    if (!m_isBusy) {
        return;
    }

    m_isCancelled = true;
    m_timeoutTimer.stop();

    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
    }
}

void InspectionWorker::onProcessReadyReadStandardOutput()
{
    m_outputBuffer.append(m_process.readAllStandardOutput());
}

void InspectionWorker::onProcessReadyReadStandardError()
{
    m_errorBuffer.append(m_process.readAllStandardError());
}

void InspectionWorker::onTimeout()
{
    if (m_isBusy && m_process.state() != QProcess::NotRunning) {
        m_isCancelled = true;
        m_process.kill();
        handleTaskCompletion(false, "Inspection task timed out after 15 seconds.");
    }
}

void InspectionWorker::onProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    if (!m_isBusy || m_isCancelled) {
        return;
    }

    m_timeoutTimer.stop();
    handleTaskCompletion(false, m_process.errorString());
}

void InspectionWorker::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_timeoutTimer.stop();

    // Read remaining bytes
    m_outputBuffer.append(m_process.readAllStandardOutput());
    m_errorBuffer.append(m_process.readAllStandardError());

    if (!m_isBusy) {
        return;
    }

    if (m_isCancelled) {
        handleTaskCompletion(false, "Task cancelled.");
        return;
    }

    const bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
    QString errorMsg;
    if (!success) {
        errorMsg = QString::fromUtf8(m_errorBuffer).trimmed();
        if (errorMsg.isEmpty()) {
            errorMsg = QString("Process exited with code %1").arg(exitCode);
        }
    }

    handleTaskCompletion(success, errorMsg);
}

void InspectionWorker::handleTaskCompletion(bool success, const QString& errorMessage)
{
    m_isBusy = false;
    const InspectionTask task = m_currentTask;

    if (task.kind == InspectionKind::Metadata) {
        ProbeResult result;
        result.id = task.id;
        result.success = success;
        result.errorMessage = errorMessage;

        if (success) {
            QString parseError;
            if (!parseProbeMetadata(m_outputBuffer, result.metadata, parseError)) {
                result.success = false;
                result.errorMessage = parseError;
            }
        }

        emit metadataFinished(result);
    } else {
        ThumbnailResult result;
        result.id = task.id;
        result.success = success;
        result.errorMessage = errorMessage;

        if (success) {
            QPixmap pix;
            if (pix.loadFromData(m_outputBuffer, "JPG")) {
                if (task.targetWidth > 0 && task.targetHeight > 0) {
                    result.thumbnail = pix.scaled(task.targetWidth, task.targetHeight,
                                                  Qt::KeepAspectRatio, Qt::SmoothTransformation);
                } else {
                    result.thumbnail = pix;
                }
            } else {
                result.success = false;
                result.errorMessage = "Failed to decode thumbnail image data.";
            }
        }

        emit thumbnailFinished(result);
    }

    emit workerFinished(m_workerId);
}

bool InspectionWorker::parseProbeMetadata(const QByteArray& jsonData, VideoMetadata& outMeta, QString& error)
{
    const QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject()) {
        error = "Invalid metadata format returned by ffprobe.";
        return false;
    }

    const QJsonObject rootObj = doc.object();

    // Streams
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

    // Format
    const QJsonObject formatObj = rootObj.value("format").toObject();
    outMeta.durationSeconds = formatObj.value("duration").toString().toDouble();
    outMeta.fileSizeBytes = formatObj.value("size").toString().toLongLong();

    // Fallback: look for duration in streams if format duration is missing
    if (outMeta.durationSeconds <= 0.0) {
        for (const QJsonValue& val : streams) {
            const QJsonObject streamObj = val.toObject();
            const double sDur = streamObj.value("duration").toString().toDouble();
            if (sDur > 0.0) {
                outMeta.durationSeconds = sDur;
                break;
            }
        }
    }

    if (outMeta.fileSizeBytes <= 0) {
        outMeta.fileSizeBytes = QFileInfo(m_currentTask.filePath).size();
    }

    return true;
}
