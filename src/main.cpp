#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStandardPaths>

#include "ui/MainWindow.h"
#include "media/MediaService.h"
#include "media/ProcessRunner.h"

static QString resolveTool(const QString& name)
{
    const QString resolved = QStandardPaths::findExecutable(name);
    return resolved.isEmpty() ? name : resolved;
}

int main(int argc, char *argv[])
{
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

    ProcessRunner ffmpeg(resolveTool("ffmpeg"));
    ProcessRunner ffprobe(resolveTool("ffprobe"));

    MediaService mediaService(ffmpeg, ffprobe);

    MainWindow window(mediaService);
    window.show();

    return app.exec();
}