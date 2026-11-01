#pragma once

#include <QString>

struct ToolStatus {
    QString executablePath;
    QString versionString;
    bool isValid = false;
};

class ExecutableLocator
{
public:
    // Locates tool using search order:
    // 1. customOverride (if provided)
    // 2. Saved user preference in QSettings
    // 3. Application bundle / binary folder (e.g. appDir or appDir/bin)
    // 4. System PATH
    static ToolStatus locateFfmpeg(const QString& customPath = QString());
    static ToolStatus locateFfprobe(const QString& customPath = QString());
    static ToolStatus locate(const QString& toolBaseName, const QString& customPath = QString());

    // Validates by running '<path> -version' and reading the version banner
    static bool validate(const QString& fullPath, QString* outVersion = nullptr);

    // Persistent settings accessors
    static QString savedCustomPath(const QString& toolBaseName);
    static void setSavedCustomPath(const QString& toolBaseName, const QString& path);
    static void clearSavedCustomPath(const QString& toolBaseName);

private:
    static QString checkCandidate(const QString& path);
};
