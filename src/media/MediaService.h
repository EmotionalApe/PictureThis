#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

class ProcessRunner;

class MediaService : public QObject
{
    Q_OBJECT

public:
    MediaService(
        ProcessRunner& ffmpeg,
        ProcessRunner& ffprobe,
        QObject* parent = nullptr
    );
    ~MediaService() override = default;

    MediaService(const MediaService&) = delete;
    MediaService& operator=(const MediaService&) = delete;

    bool createVideoFromImageAndAudio(
        const QString& imagePath,
        const QString& audioPath,
        const QString& outputPath,
        QString* errorMessage = nullptr
    );

    bool getMediaDuration(
        const QString& filePath,
        double& duration,
        QString* errorMessage = nullptr
    );

    bool isProcessing() const;
    void cancel();

signals:
    void videoCreationStarted();
    void progressChanged(int percentage);
    void videoCreationCancelled();
    void videoCreationFinished(bool success, const QString& errorMessage);

private slots:
    void onFfmpegFinished(int exitCode, QProcess::ExitStatus exitStatus, const QString& stdoutStr, const QString& stderrStr);
    void onFfmpegError(const QString& errorMessage);
    void onStandardOutputLineReceived(const QString& line);

private:

    ProcessRunner& m_ffmpeg;
    ProcessRunner& m_ffprobe;
    bool m_isProcessing = false;
    bool m_isCancelling = false;
    double m_audioDuration = 0.0;
    QString m_currentOutputPath;
};