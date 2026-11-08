#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

class ProcessRunner : public QObject
{
    Q_OBJECT

public:
    explicit ProcessRunner(QString executable, QObject* parent = nullptr);
    ~ProcessRunner() override;

    ProcessRunner(const ProcessRunner&) = delete;
    ProcessRunner& operator=(const ProcessRunner&) = delete;

    // Asynchronous execution
    bool start(const QStringList& arguments);

    // Synchronous execution (for fast probes like ffprobe)
    bool run(
        const QStringList& arguments,
        QString* standardOutput = nullptr,
        QString* standardError = nullptr
    );

    // Synchronous binary execution (for image/thumbnail extraction)
    bool runBinary(
        const QStringList& arguments,
        QByteArray* standardOutput = nullptr,
        QString* standardError = nullptr
    );

    bool isRunning() const;
    void cancel();

    QString executable() const;
    void setExecutable(const QString& executable);

signals:
    void started();
    void finished(int exitCode, QProcess::ExitStatus exitStatus, const QString& standardOutput, const QString& standardError);
    void errorOccurred(const QString& errorMessage);
    void standardOutputLineReceived(const QString& line);
    void standardErrorLineReceived(const QString& line);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onGracefulTimeout();
    void onTerminateTimeout();

private:
    // Cancel escalation state machine: Idle → AwaitGraceful → AwaitTerminate → AwaitKill
    enum class CancelStage { Idle, AwaitGraceful, AwaitTerminate, AwaitKill };

    QString m_executable;
    QProcess m_process;
    QString m_standardOutput;
    QString m_standardError;
    QTimer m_gracefulTimer;    // fires if 'q' doesn't elicit exit within 3 s
    QTimer m_terminateTimer;   // fires if SIGTERM doesn't elicit exit within 1 s
    CancelStage m_cancelStage = CancelStage::Idle;
};