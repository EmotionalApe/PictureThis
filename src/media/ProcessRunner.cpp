#include "ProcessRunner.h"

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

bool ProcessRunner::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

void ProcessRunner::cancel()
{
    if (!isRunning()) {
        return;
    }

#if defined(Q_OS_WIN)
    // On Windows, console apps lack a message pump for WM_CLOSE, so terminate()
    // does not stop them. kill() terminates the process immediately.
    m_process.kill();
    m_process.waitForFinished(300);
#else
    m_process.terminate();
    if (!m_process.waitForFinished(500)) {
        m_process.kill();
        m_process.waitForFinished(300);
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