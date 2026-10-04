#include <QCoreApplication>
#include <QDebug>

#include "media/ProcessRunner.h"
#include "media/MediaService.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    ProcessRunner ffmpeg("ffmpeg.exe");
    ProcessRunner ffprobe("ffprobe.exe");

    MediaService media(ffmpeg, ffprobe);

    QString error;

    const bool success = media.createVideoFromImageAndAudio(
        R"(C:\Users\maruf\Downloads\image.jpeg)",
        R"(C:\Users\maruf\Downloads\audio_test.m4a)",
        R"(C:\Users\maruf\Downloads\cpp_output.mp4)",
        &error
    );

    if (success) {
        qDebug() << "Video created successfully.";
        return 0;
    }

    qDebug().noquote() << "Failed:";
    qDebug().noquote() << error;

    return 1;
}