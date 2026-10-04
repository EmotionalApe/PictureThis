#pragma once

#include <QString>
#include <QStringList>

class ProcessRunner
{
public:
    explicit ProcessRunner(QString executable);

    bool run(
        const QStringList& arguments,
        QString* standardOutput = nullptr,
        QString* standardError = nullptr
    );

private:
    QString m_executable;
};