#include "ExecutableLocator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

namespace {
    const char* const kSettingsOrg = "PictureThis";
    const char* const kSettingsApp = "PictureThis";
    const char* const kSettingsPrefix = "tools/";

    QSettings getSettings()
    {
        return QSettings(QSettings::IniFormat, QSettings::UserScope, kSettingsOrg, kSettingsApp);
    }
}

ToolStatus ExecutableLocator::locateFfmpeg(const QString& customPath)
{
    return locate("ffmpeg", customPath);
}

ToolStatus ExecutableLocator::locateFfprobe(const QString& customPath)
{
    return locate("ffprobe", customPath);
}

ToolStatus ExecutableLocator::locate(const QString& toolBaseName, const QString& customPath)
{
    ToolStatus status;

    // 1. Check custom override if provided
    if (!customPath.trimmed().isEmpty()) {
        const QString candidate = checkCandidate(customPath.trimmed());
        if (!candidate.isEmpty()) {
            QString version;
            if (validate(candidate, &version)) {
                status.executablePath = candidate;
                status.versionString = version;
                status.isValid = true;
                return status;
            }
        }
    }

    // 2. Check saved user preference in QSettings
    const QString saved = savedCustomPath(toolBaseName);
    if (!saved.isEmpty()) {
        const QString candidate = checkCandidate(saved);
        if (!candidate.isEmpty()) {
            QString version;
            if (validate(candidate, &version)) {
                status.executablePath = candidate;
                status.versionString = version;
                status.isValid = true;
                return status;
            }
        }
    }

    // 3. Check application directory and local binary subdirectories
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList localSearchPaths = {
        appDir + "/" + toolBaseName,
        appDir + "/bin/" + toolBaseName,
        appDir + "/../bin/" + toolBaseName
    };

    for (const QString& path : localSearchPaths) {
        const QString candidate = checkCandidate(path);
        if (!candidate.isEmpty()) {
            QString version;
            if (validate(candidate, &version)) {
                status.executablePath = candidate;
                status.versionString = version;
                status.isValid = true;
                return status;
            }
        }
    }

    // 4. Check system PATH via QStandardPaths
    const QString systemPath = QStandardPaths::findExecutable(toolBaseName);
    if (!systemPath.isEmpty()) {
        QString version;
        if (validate(systemPath, &version)) {
            status.executablePath = systemPath;
            status.versionString = version;
            status.isValid = true;
            return status;
        }
    }

    // 5. Tool could not be found or failed validation
    status.executablePath.clear();
    status.versionString.clear();
    status.isValid = false;
    return status;
}

bool ExecutableLocator::validate(const QString& fullPath, QString* outVersion)
{
    const QString trimmed = fullPath.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    QString executableToRun = trimmed;
    QFileInfo fileInfo(executableToRun);

    // If it's not an absolute path to an existing file, try resolving via PATH
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        const QString fromPath = QStandardPaths::findExecutable(trimmed);
        if (!fromPath.isEmpty()) {
            executableToRun = fromPath;
            fileInfo = QFileInfo(executableToRun);
        } else {
            return false;
        }
    }

    QProcess process;
    process.start(executableToRun, QStringList{"-version"});
    if (!process.waitForStarted(2000)) {
        return false;
    }

    if (!process.waitForFinished(3000)) {
        process.kill();
        process.waitForFinished(1000);
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return false;
    }

    const QString stdoutStr = QString::fromUtf8(process.readAllStandardOutput());
    const QStringList lines = stdoutStr.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        return false;
    }

    const QString firstLine = lines.first().trimmed();
    if (!firstLine.contains("version", Qt::CaseInsensitive)) {
        return false;
    }

    if (outVersion) {
        *outVersion = firstLine;
    }
    return true;
}

QString ExecutableLocator::savedCustomPath(const QString& toolBaseName)
{
    QSettings settings = getSettings();
    return settings.value(QString(kSettingsPrefix) + toolBaseName + "Path").toString();
}

void ExecutableLocator::setSavedCustomPath(const QString& toolBaseName, const QString& path)
{
    QSettings settings = getSettings();
    if (path.trimmed().isEmpty()) {
        settings.remove(QString(kSettingsPrefix) + toolBaseName + "Path");
    } else {
        settings.setValue(QString(kSettingsPrefix) + toolBaseName + "Path", path.trimmed());
    }
}

void ExecutableLocator::clearSavedCustomPath(const QString& toolBaseName)
{
    QSettings settings = getSettings();
    settings.remove(QString(kSettingsPrefix) + toolBaseName + "Path");
}

QString ExecutableLocator::checkCandidate(const QString& path)
{
    if (path.trimmed().isEmpty()) {
        return QString();
    }

    const QString cleaned = QDir::cleanPath(path.trimmed());
    QFileInfo info(cleaned);
    if (info.exists() && info.isFile()) {
        return info.absoluteFilePath();
    }

#ifdef Q_OS_WIN
    if (!cleaned.endsWith(".exe", Qt::CaseInsensitive)) {
        const QString withExe = cleaned + ".exe";
        QFileInfo exeInfo(withExe);
        if (exeInfo.exists() && exeInfo.isFile()) {
            return exeInfo.absoluteFilePath();
        }
    }
#endif

    return QString();
}
