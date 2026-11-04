#pragma once

#include <QWidget>
#include <QPixmap>
#include <QTime>
#include "../media/MediaService.h"
#include "../media/InspectionTypes.h"

class InspectionScheduler;
class QLineEdit;
class QLabel;
class QPushButton;
class QProgressBar;
class QTimeEdit;
class QCheckBox;
class QComboBox;
class QTimer;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QResizeEvent;
class RangeTimelineWidget;

class VideoTrimWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoTrimWidget(
        MediaService& mediaService,
        InspectionScheduler& scheduler,
        QWidget* parent = nullptr
    );
    ~VideoTrimWidget() override = default;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void chooseVideo();
    void chooseOutput();
    void trimVideo();
    void cancelTrim();
    void onClearClicked();

    void onVideoPathChanged(const QString& path);
    void onStartTimeChanged(const QTime& time);
    void onEndTimeChanged(const QTime& time);
    void onResetStartToBeginning();
    void onResetEndToTotal();
    void onThumbnailDebounceTimeout();

    void onVideoTrimStarted();
    void onProgressChanged(int percentage);
    void onVideoTrimCancelled();
    void onVideoTrimFinished(bool success, const QString& errorMessage);

    void onFormatChanged(int index);
    void onGifExportStarted();
    void onGifExportCancelled();
    void onGifExportFinished(bool success, const QString& errorMessage);

    // InspectionScheduler signals
    void onInspectionMetadataReady(const ProbeResult& result);
    void onInspectionThumbnailReady(const ThumbnailResult& result);

private:
    void setUiBusy(bool busy);
    void loadVideo(const QString& path);
    void updateValidationAndSummary();
    void requestThumbnailUpdate();
    void updateThumbnails();
    void renderThumbnails();

    static QTime secondsToQTime(double seconds);
    static double qTimeToSeconds(const QTime& time);
    static QString formatDuration(double seconds);

    MediaService& m_mediaService;
    InspectionScheduler& m_scheduler;
    VideoMetadata m_currentMetadata;
    bool m_videoLoaded = false;

    RequestId m_pendingMetadataId;
    RequestId m_pendingStartThumbId;
    RequestId m_pendingEndThumbId;

    // Dual thumbnail preview & timeline
    QWidget* m_placeholderCard;
    QPushButton* m_placeholderAddButton;
    QWidget* m_thumbnailsContainer;
    QWidget* m_previewToolbar;
    QPushButton* m_chooseVideoButton;
    QPushButton* m_clearButton;
    QLabel* m_startThumbnailLabel;
    QLabel* m_startCaptionLabel;
    QLabel* m_endThumbnailLabel;
    QLabel* m_endCaptionLabel;
    QPixmap m_startPixmap;
    QPixmap m_endPixmap;

    QLabel* m_metadataLabel;
    RangeTimelineWidget* m_timelineWidget;

    // Inputs & time controls
    QLineEdit* m_videoPath;
    QPushButton* m_videoButton;

    QTimeEdit* m_startTimeEdit;
    QPushButton* m_startFromBeginningButton;

    QTimeEdit* m_endTimeEdit;
    QPushButton* m_endToEndButton;

    QLabel* m_trimSummaryLabel;

    // Export Format & Mode Controls
    QComboBox* m_formatComboBox;
    QWidget* m_mp4OptionsWidget;
    QCheckBox* m_reencodeCheckBox;

    QWidget* m_gifOptionsWidget;
    QComboBox* m_gifFpsComboBox;
    QComboBox* m_gifResolutionComboBox;
    QCheckBox* m_gifLoopCheckBox;

    QLineEdit* m_outputPath;
    QPushButton* m_outputButton;

    // Actions & Progress
    QPushButton* m_trimButton;
    QPushButton* m_cancelButton;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;

    // Debounce timer for smooth thumbnail extraction
    QTimer* m_thumbnailTimer;

    // Monotonically increasing serial number for stale-result detection.
    // Incremented every time a new thumbnail request is issued. A result
    // captured under an older serial is silently discarded so that a
    // thumbnail from a superseded trim position can never overwrite the
    // current preview.
    quint64 m_thumbSerial = 0;
};
