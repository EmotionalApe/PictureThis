#include "ProcessRunner.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

ProcessRunner::ProcessRunner(QString executable, QObject* parent)
    : QObject(parent),
      m_executable(std::move(executable)),
      m_process(this)
{
    connect(&m_process, &QProcess::started,
            this, &ProcessRunner::started);

    connect(&m_process, &QProcess::finished,
            this, &ProcessRunner::onProcessFinished);

    connect(&m_process, &QProcess::errorOccurred,
            this, &ProcessRunner::onProcessError);

    connect(&m_process, &QProcess::readyReadStandardOutput,
            this, &ProcessRunner::onReadyReadStandardOutput);

    connect(&m_process, &QProcess::readyReadStandardError,
            this, &ProcessRunner::onReadyReadStandardError);

    // --- Cancel escalation timers (single-shot; wired to escalation slots) ---
    m_gracefulTimer.setSingleShot(true);
    m_gracefulTimer.setInterval(3000); // 3 s for ffmpeg to respond to 'q'
    connect(&m_gracefulTimer, &QTimer::timeout,
            this, &ProcessRunner::onGracefulTimeout);

    m_terminateTimer.setSingleShot(true);
    m_terminateTimer.setInterval(1000); // 1 s for the process to respond to SIGTERM
    connect(&m_terminateTimer, &QTimer::timeout,
            this, &ProcessRunner::onTerminateTimeout);
}

ProcessRunner::~ProcessRunner()
{
    // Stop timers first so no escalation callbacks fire after destruction begins.
    m_gracefulTimer.stop();
    m_terminateTimer.stop();

    if (!isRunning()) {
        return;
    }

    // The destructor must be synchronous: QProcess is a value member and must be
    // fully stopped before it is destroyed. The public cancel() is non-blocking
    // by design; here we cannot afford that — so we do a fast synchronous teardown.
    m_process.write("q");
    m_process.closeWriteChannel();
    if (!m_process.waitForFinished(1500)) {
        m_process.kill();
        m_process.waitForFinished(500);
    }
}

bool ProcessRunner::start(const QStringList& arguments)
{
    if (isRunning()) {
        return false;
    }

    // Reset any stale cancel state from a previous run
    m_cancelStage = CancelStage::Idle;
    m_gracefulTimer.stop();
    m_terminateTimer.stop();

    m_standardOutput.clear();
    m_standardError.clear();

    // Keep stdin open so we can send 'q' for a graceful ffmpeg shutdown on cancel.
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start(m_executable, arguments);
    return true;
}

bool ProcessRunner::run(
    const QStringList& arguments,
    QString* standardOutput,
    QString* standardError)
{
    QProcess process;
    process.start(m_executable, arguments);

    if (!process.waitForStarted()) {
        if (standardError) {
            *standardError = process.errorString();
        }
        return false;
    }

    process.waitForFinished(-1);

    if (standardOutput) {
        *standardOutput = QString::fromUtf8(process.readAllStandardOutput());
    }

    if (standardError) {
        *standardError = QString::fromUtf8(process.readAllStandardError());
    }

    return process.exitStatus() == QProcess::NormalExit &&
           process.exitCode() == 0;
}

bool ProcessRunner::runBinary(
    const QStringList& arguments,
    QByteArray* standardOutput,
    QString* standardError)
{
    QProcess process;
    process.start(m_executable, arguments);

    if (!process.waitForStarted()) {
        if (standardError) {
            *standardError = process.errorString();
        }
        return false;
    }

    process.waitForFinished(10000);

    if (standardOutput) {
        *standardOutput = process.readAllStandardOutput();
    }

    if (standardError) {
        *standardError = QString::fromUtf8(process.readAllStandardError());
    }

    return process.exitStatus() == QProcess::NormalExit &&
           process.exitCode() == 0;
}

bool ProcessRunner::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

void ProcessRunner::cancel()
{
    if (!isRunning() || m_cancelStage != CancelStage::Idle) {
        return; // already stopped, or escalation already in progress
    }

    // Stage 1: ask ffmpeg to stop gracefully via its stdin 'q' protocol.
    // ffmpeg flushes and closes the output file cleanly when it reads 'q'.
    m_cancelStage = CancelStage::AwaitGraceful;
    m_process.write("q");
    m_process.closeWriteChannel();

    // If ffmpeg has not exited within 3 s, onGracefulTimeout() escalates to SIGTERM.
    m_gracefulTimer.start();
}

QString ProcessRunner::executable() const
{
    return m_executable;
}

void ProcessRunner::setExecutable(const QString& executable)
{
    if (isRunning()) {
        return;
    }
    m_executable = executable;
}

void ProcessRunner::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // A process exit — regardless of cause — always terminates the cancel ladder.
    m_gracefulTimer.stop();
    m_terminateTimer.stop();
    m_cancelStage = CancelStage::Idle;

    // Flush any remaining lines from buffers
    while (m_process.canReadLine()) {
        const QString line = QString::fromUtf8(m_process.readLine()).trimmed();
        if (!line.isEmpty()) {
            m_standardOutput.append(line + QLatin1Char('\n'));
            emit standardOutputLineReceived(line);
        }
    }

    const QString remainingOut = QString::fromUtf8(m_process.readAllStandardOutput());
    if (!remainingOut.isEmpty()) {
        m_standardOutput.append(remainingOut);
        emit standardOutputLineReceived(remainingOut.trimmed());
    }

    const QString remainingErr = QString::fromUtf8(m_process.readAllStandardError());
    if (!remainingErr.isEmpty()) {
        m_standardError.append(remainingErr);
        emit standardErrorLineReceived(remainingErr.trimmed());
    }

    emit finished(exitCode, exitStatus, m_standardOutput, m_standardError);
}

void ProcessRunner::onProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    emit errorOccurred(m_process.errorString());
}

void ProcessRunner::onReadyReadStandardOutput()
{
    while (m_process.canReadLine()) {
        const QString line = QString::fromUtf8(m_process.readLine()).trimmed();
        if (!line.isEmpty()) {
            m_standardOutput.append(line + QLatin1Char('\n'));
            emit standardOutputLineReceived(line);
        }
    }
}

void ProcessRunner::onReadyReadStandardError()
{
    while (m_process.canReadLine()) {
        const QString line = QString::fromUtf8(m_process.readLine()).trimmed();
        if (!line.isEmpty()) {
            m_standardError.append(line + QLatin1Char('\n'));
            emit standardErrorLineReceived(line);
        }
    }
}

void ProcessRunner::onGracefulTimeout()
{
    // Stage 2: 'q' timed out — escalate to SIGTERM (POSIX) or immediate kill (Windows).
    if (m_process.state() == QProcess::NotRunning) {
        return; // raced: process already exited before the timer fired
    }

    m_cancelStage = CancelStage::AwaitTerminate;

#if defined(Q_OS_WIN)
    // Windows does not have SIGTERM. Qt's terminate() maps to TerminateProcess(),
    // which is effectively a force-kill. Skip directly to AwaitKill stage.
    // Note: this does not propagate to child processes spawned by ffmpeg; a
    // taskkill /F /T approach would be needed for that, but keeping the cancel
    // path non-blocking takes priority here.
    m_cancelStage = CancelStage::AwaitKill;
    m_process.kill();
#else
    // SIGTERM gives ffmpeg one last chance to flush its output cleanly.
    m_process.terminate();
    // If SIGTERM is ignored or the process is stuck, onTerminateTimeout() sends SIGKILL.
    m_terminateTimer.start();
#endif
}

void ProcessRunner::onTerminateTimeout()
{
    // Stage 3 (POSIX only): SIGTERM timed out — escalate to SIGKILL.
    if (m_process.state() == QProcess::NotRunning) {
        return; // raced: process already exited before the timer fired
    }

    // SIGKILL cannot be caught or ignored — the process will be terminated unconditionally.
    m_cancelStage = CancelStage::AwaitKill;
    m_process.kill();
}