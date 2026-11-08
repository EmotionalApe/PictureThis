#include <QApplication>
#include <QColor>
#include <QPalette>

#include "ui/MainWindow.h"
#include "media/MediaService.h"
#include "media/ProcessRunner.h"
#include "media/ExecutableLocator.h"
#include "media/InspectionScheduler.h"

int main(int argc, char *argv[])
{
#ifndef Q_OS_WIN
    // On GNOME/Wayland on modern Linux (e.g. Ubuntu 24.04 / GNOME 46+),
    // Qt's default gtk3 platform theme initializes GDK which queries the
    // removed 'antialiasing' key from 'org.gnome.settings-daemon.plugins.xsettings',
    // crashing with GLib-GIO-ERROR.
    // Route platform theming through xdgdesktopportal and ensure GDK falls back to X11.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORMTHEME")) {
        qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
    }
    if (!qEnvironmentVariableIsSet("GDK_BACKEND")) {
        qputenv("GDK_BACKEND", "x11");
    }
#endif

    QApplication app(argc, argv);

    // Override the system highlight/accent color with the app's teal brand color
    // so selections, focus rings, and list highlights are consistent everywhere.
    QPalette palette = app.palette();
    const QColor teal(0x0d, 0x94, 0x88);          // #0d9488
    const QColor tealLight(0x0f, 0xb4, 0xa8);     // lighter variant for disabled/inactive
    palette.setColor(QPalette::Highlight,         teal);
    palette.setColor(QPalette::HighlightedText,   Qt::white);
    palette.setColor(QPalette::Inactive, QPalette::Highlight,       tealLight);
    palette.setColor(QPalette::Inactive, QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Highlight,       tealLight);
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);

    // Resolve FFmpeg and FFprobe executables via ExecutableLocator
    // (checking custom preferences, local bundled/app folder, and system PATH)
    const ToolStatus ffmpegStatus = ExecutableLocator::locateFfmpeg();
    const ToolStatus ffprobeStatus = ExecutableLocator::locateFfprobe();

    const QString ffmpegPath = ffmpegStatus.isValid ? ffmpegStatus.executablePath : "ffmpeg";
    const QString ffprobePath = ffprobeStatus.isValid ? ffprobeStatus.executablePath : "ffprobe";

    ProcessRunner ffmpeg(ffmpegPath);
    ProcessRunner ffprobe(ffprobePath);

    MediaService mediaService(ffmpeg, ffprobe);
    InspectionScheduler inspectionScheduler(ffmpegPath, ffprobePath, 2);

    MainWindow window(mediaService, inspectionScheduler);
    window.show();

    return app.exec();
}