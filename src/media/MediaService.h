#pragma once

#include <QObject>
#include <QPixmap>
#include <QProcess>
#include <QString>

class ProcessRunner;

struct VideoMetadata {
    double durationSeconds = 0.0;
    int width = 0;
    int height = 0;
    QString videoCodec;
    QString audioCodec;
    qint64 fileSizeBytes = 0;
};

struct GifOptions {
    int fps = 15;
    int targetWidth = 480; // <= 0 for original width
    bool infiniteLoop = true;
};

struct VideoCompressOptions {
    double targetSizeBytes = 0.0;
    int targetHeight = 0;       // 0 for original, 1080, 720, 480, 360
    int audioBitrateKbps = 0;   // 0 for auto
    bool twoPass = true;
};

enum class MediaJobType {
    None,
    ImageToVideo,
    VideoTrim,
    VideoCombine,
    GifExport,
    VideoCompress
};

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

    bool trimVideo(
        const QString& inputPath,
        double startTime,
        double endTime,
        const QString& outputPath,
        bool accurateReencode,
        QString* errorMessage = nullptr
    );

    bool combineVideos(
        const QStringList& inputPaths,
        const QString& outputPath,
        bool forceReencode,
        QString* errorMessage = nullptr
    );

    bool exportGif(
        const QString& inputPath,
        double startTime,
        double endTime,
        const QString& outputPath,
        const GifOptions& options,
        QString* errorMessage = nullptr
    );

    bool compressVideo(
        const QString& inputPath,
        const QString& outputPath,
        const VideoCompressOptions& options,
        QString* errorMessage = nullptr
    );

    bool getMediaDuration(
        const QString& filePath,
        double& duration,
        QString* errorMessage = nullptr
    );

    bool getVideoMetadata(
        const QString& filePath,
        VideoMetadata& outMeta,
        QString* errorMessage = nullptr
    );

    bool extractThumbnail(
        const QString& filePath,
        double timestampSeconds,
        QPixmap& outPixmap,
        QString* errorMessage = nullptr
    );

    bool isProcessing() const;
    void cancel();

    void setFfmpegExecutable(const QString& path);
    void setFfprobeExecutable(const QString& path);
    QString ffmpegExecutable() const;
    QString ffprobeExecutable() const;

signals:
    // Image to Video signals
    void videoCreationStarted();
    void videoCreationCancelled();
    void videoCreationFinished(bool success, const QString& errorMessage);

    // Video Trimming signals
    void videoTrimStarted();
    void videoTrimCancelled();
    void videoTrimFinished(bool success, const QString& errorMessage);

    // Video Combining signals
    void videoCombineStarted();
    void videoCombineCancelled();
    void videoCombineFinished(bool success, const QString& errorMessage);

    // GIF Export signals
    void gifExportStarted();
    void gifExportCancelled();
    void gifExportFinished(bool success, const QString& errorMessage);

    // Video Compression signals
    void videoCompressStarted();
    void videoCompressCancelled();
    void videoCompressFinished(bool success, const QString& errorMessage);
    void compressStatusChanged(const QString& statusText);

    // Shared progress signal
    void progressChanged(int percentage);

private slots:
    void onFfmpegFinished(int exitCode, QProcess::ExitStatus exitStatus, const QString& stdoutStr, const QString& stderrStr);
    void onFfmpegError(const QString& errorMessage);
    void onStandardOutputLineReceived(const QString& line);

private:
    void handleJobCompletion(bool success, const QString& errorMessage);
    void handleJobCancellation();
    void cleanupTemporaryFiles();

    ProcessRunner& m_ffmpeg;
    ProcessRunner& m_ffprobe;
    bool m_isProcessing = false;
    bool m_isCancelling = false;
    MediaJobType m_currentJob = MediaJobType::None;
    double m_targetDuration = 0.0;
    QString m_currentOutputPath;
    QString m_concatManifestPath;

    // Multi-pass compression state
    int m_compressPass = 0;
    QString m_passLogPrefix;
    QStringList m_compressPass2Args;
    double m_compressSourceDuration = 0.0;
};