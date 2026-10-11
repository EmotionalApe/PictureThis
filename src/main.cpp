#include <QApplication>
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

    ProcessRunner ffmpeg(resolveTool("ffmpeg"));
    ProcessRunner ffprobe(resolveTool("ffprobe"));

    MediaService mediaService(ffmpeg, ffprobe);

    MainWindow window(mediaService);
    window.show();

    return app.exec();
}