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
}

ProcessRunner::~ProcessRunner()
{
    cancel();
}

bool ProcessRunner::start(const QStringList& arguments)
{
    if (isRunning()) {
        return false;
    }

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
    if (!isRunning()) {
        return;
    }

    // Ask ffmpeg to stop gracefully by sending 'q' to its stdin.
    // ffmpeg monitors stdin for this and will flush and close the output file cleanly.
    m_process.write("q");
    m_process.closeWriteChannel();

    if (m_process.waitForFinished(3000)) {
        // ffmpeg exited cleanly within 3 seconds — done.
        return;
    }

    // ffmpeg didn't respond to 'q' in time — force-kill the entire process tree.
#if defined(Q_OS_WIN)
    // taskkill /F /T kills the process AND all its children, which handles
    // the case where ffmpeg has spawned child processes (e.g. sub-encoders).
    const qint64 pid = m_process.processId();
    if (pid > 0) {
        QProcess::execute(
            "taskkill",
            {"/F", "/T", "/PID", QString::number(pid)}
        );
    }
    m_process.waitForFinished(2000);
#else
    m_process.terminate();
    if (!m_process.waitForFinished(1000)) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
#endif
}

QString ProcessRunner::executable() const
{
    return m_executable;
}

void ProcessRunner::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
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