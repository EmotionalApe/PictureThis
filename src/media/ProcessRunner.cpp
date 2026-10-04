#include "ProcessRunner.h"

#include <QProcess>

ProcessRunner::ProcessRunner(QString executable)
    : m_executable(std::move(executable))
{
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