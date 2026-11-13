#pragma once

#include <QObject>
#include <QProcess>
#include <QByteArray>
#include <QTimer>
#include "InspectionTypes.h"

class InspectionWorker : public QObject
{
    Q_OBJECT

public:
    explicit InspectionWorker(int workerId, QObject* parent = nullptr);
    ~InspectionWorker() override;

    InspectionWorker(const InspectionWorker&) = delete;
    InspectionWorker& operator=(const InspectionWorker&) = delete;

    bool isBusy() const;
    int workerId() const;
    const InspectionTask& currentTask() const;

    bool startTask(const InspectionTask& task, const QString& executablePath);
    void cancel();

signals:
    void metadataFinished(const ProbeResult& result);
    void thumbnailFinished(const ThumbnailResult& result);
    void workerFinished(int workerId);

private slots:
    void onProcessReadyReadStandardOutput();
    void onProcessReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onTimeout();

private:
    void handleTaskCompletion(bool success, const QString& errorMessage);
    bool parseProbeMetadata(const QByteArray& jsonData, VideoMetadata& outMeta, QString& error);

    int m_workerId;
    QProcess m_process;
    QTimer m_timeoutTimer;
    InspectionTask m_currentTask;
    bool m_isBusy = false;
    bool m_isCancelled = false;
    QByteArray m_outputBuffer;
    QByteArray m_errorBuffer;
};
