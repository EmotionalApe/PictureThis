#pragma once

#include <QWidget>
#include <QPixmap>
#include "../media/MediaService.h"

class QLineEdit;
class QLabel;
class QPushButton;
class QProgressBar;
class QSlider;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QResizeEvent;

class VideoCompressWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoCompressWidget(
        MediaService& mediaService,
        QWidget* parent = nullptr
    );
    ~VideoCompressWidget() override = default;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void chooseVideo();
    void chooseOutput();
    void compressVideo();
    void cancelCompression();

    void onVideoPathChanged(const QString& path);
    void onPercentageSliderChanged(int value);
    void onPercentageSpinBoxChanged(int value);
    void onPresetButtonClicked(int percentage);
    void onResolutionChanged(int index);
    void onAudioQualityChanged(int index);

    void onVideoCompressStarted();
    void onProgressChanged(int percentage);
    void onCompressStatusChanged(const QString& statusText);
    void onVideoCompressCancelled();
    void onVideoCompressFinished(bool success, const QString& errorMessage);

private:
    void setUiBusy(bool busy);
    void loadVideo(const QString& path);
    void updateCompressionCalculations();
    void renderThumbnail();

    static QString formatFileSize(qint64 bytes);
    static QString formatDuration(double seconds);

    MediaService& m_mediaService;
    VideoMetadata m_currentMetadata;
    bool m_videoLoaded = false;
    QPixmap m_sourceThumbnailPixmap;

    // Left Column Preview & Summary
    QWidget* m_placeholderCard;
    QWidget* m_previewContainer;
    QLabel* m_thumbnailLabel;
    QLabel* m_sourceMetadataLabel;
    QLabel* m_compressionSummaryLabel;
    QLabel* m_advisoryLabel;

    // Right Column Controls
    QLineEdit* m_videoPath;
    QPushButton* m_videoButton;

    QPushButton* m_preset75Btn;
    QPushButton* m_preset50Btn;
    QPushButton* m_preset25Btn;
    QPushButton* m_preset10Btn;

    QSlider* m_percentageSlider;
    QSpinBox* m_percentageSpinBox;
    QLabel* m_targetSizeBadge;

    QComboBox* m_resolutionComboBox;
    QComboBox* m_audioQualityComboBox;
    QCheckBox* m_twoPassCheckBox;

    QLineEdit* m_outputPath;
    QPushButton* m_outputButton;

    QPushButton* m_compressButton;
    QPushButton* m_cancelButton;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
};
