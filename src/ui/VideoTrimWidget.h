#pragma once

#include <QWidget>
#include <QPixmap>
#include <QTime>
#include "../media/MediaService.h"

class QLineEdit;
class QLabel;
class QPushButton;
class QProgressBar;
class QTimeEdit;
class QCheckBox;
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
    VideoMetadata m_currentMetadata;
    bool m_videoLoaded = false;

    // Dual thumbnail preview & timeline
    QWidget* m_placeholderCard;
    QWidget* m_thumbnailsContainer;
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
    QCheckBox* m_reencodeCheckBox;

    QLineEdit* m_outputPath;
    QPushButton* m_outputButton;

    // Actions & Progress
    QPushButton* m_trimButton;
    QPushButton* m_cancelButton;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;

    // Debounce timer for smooth thumbnail extraction
    QTimer* m_thumbnailTimer;
};
