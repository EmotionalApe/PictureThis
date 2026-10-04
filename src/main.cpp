#include <QApplication>

#include "ui/MainWindow.h"
#include "media/MediaService.h"
#include "media/ProcessRunner.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    ProcessRunner ffmpeg("ffmpeg.exe");
    ProcessRunner ffprobe("ffprobe.exe");

    MediaService mediaService(ffmpeg, ffprobe);

    MainWindow window(mediaService);
    window.show();

    return app.exec();
}