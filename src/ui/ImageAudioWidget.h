#pragma once

#include <QWidget>
#include <QPixmap>

class MediaService;
class QLineEdit;
class QLabel;
class QPushButton;
class QProgressBar;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QResizeEvent;

class ImageAudioWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageAudioWidget(
        MediaService& mediaService,
        QWidget* parent = nullptr
    );
    ~ImageAudioWidget() override;

    void cleanupPastedTempFile();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void chooseImage();
    void chooseAudio();
    void chooseOutput();
    void pasteImageFromClipboard();
    void createVideo();
    void cancelVideo();

    void onImagePathChanged(const QString& path);
    void onAudioPathChanged(const QString& path);

    void onVideoCreationStarted();
    void onProgressChanged(int percentage);
    void onVideoCreationCancelled();
    void onVideoCreationFinished(bool success, const QString& errorMessage);

private:
    void setUiBusy(bool busy);
    void setImagePath(const QString& path);
    void setAudioPath(const QString& path);
    void updateImagePreview(const QString& path);
    void updateAudioDetails(const QString& path);
    void renderPreviewThumbnail();

    MediaService& m_mediaService;

    // Image preview & info
    QLabel* m_previewLabel;
    QLabel* m_imageInfoLabel;
    QPixmap m_loadedPixmap;

    // Form inputs and buttons
    QLineEdit* m_imagePath;
    QLineEdit* m_audioPath;
    QLineEdit* m_outputPath;

    QPushButton* m_imageButton;
    QPushButton* m_pasteButton;
    QPushButton* m_audioButton;
    QLabel* m_audioInfoLabel;
    QPushButton* m_outputButton;
    QPushButton* m_createButton;
    QPushButton* m_cancelButton;

    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;

    // Pasted temp file management
    QString m_pastedTempFilePath;
};
