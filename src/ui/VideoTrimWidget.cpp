#include "VideoTrimWidget.h"
#include "RangeTimelineWidget.h"
#include "../media/MediaService.h"

#include <algorithm>
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimeEdit>
#include <QCheckBox>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

VideoTrimWidget::VideoTrimWidget(
    MediaService& mediaService,
    QWidget* parent)
    : QWidget(parent),
      m_mediaService(mediaService),
      m_placeholderCard(new QWidget(this)),
      m_thumbnailsContainer(new QWidget(this)),
      m_startThumbnailLabel(new QLabel(this)),
      m_startCaptionLabel(new QLabel("Start: 00:00:00", this)),
      m_endThumbnailLabel(new QLabel(this)),
      m_endCaptionLabel(new QLabel("End: 00:00:00", this)),
      m_metadataLabel(new QLabel("No video loaded", this)),
      m_timelineWidget(new RangeTimelineWidget(this)),
      m_videoPath(new QLineEdit(this)),
      m_videoButton(new QPushButton("Browse...", this)),
      m_startTimeEdit(new QTimeEdit(this)),
      m_startFromBeginningButton(new QPushButton("From Start", this)),
      m_endTimeEdit(new QTimeEdit(this)),
      m_endToEndButton(new QPushButton("To End", this)),
      m_trimSummaryLabel(new QLabel("Trim Duration: 00:00", this)),
      m_reencodeCheckBox(new QCheckBox("Accurate re-encode (frame-exact cut, slower)", this)),
      m_outputPath(new QLineEdit(this)),
      m_outputButton(new QPushButton("Browse...", this)),
      m_trimButton(new QPushButton("Trim Video", this)),
      m_cancelButton(new QPushButton("Cancel", this)),
      m_progressBar(new QProgressBar(this)),
      m_statusLabel(new QLabel("Ready", this)),
      m_thumbnailTimer(new QTimer(this))
{
    setAcceptDrops(true);

    m_thumbnailTimer->setSingleShot(true);
    m_thumbnailTimer->setInterval(250);

    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(20);

    // ==================== LEFT COLUMN: PREVIEWS & TIMELINE ====================
    auto* leftWidget = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    auto* previewHeading = new QLabel("Video Preview & Timeline", this);
    previewHeading->setStyleSheet("font-weight: bold; font-size: 13px;");
    leftLayout->addWidget(previewHeading);

    // Placeholder Card (shown when no video is loaded)
    auto* placeholderLayout = new QVBoxLayout(m_placeholderCard);
    placeholderLayout->setContentsMargins(0, 0, 0, 0);
    auto* placeholderLabel = new QLabel(
        "No video selected\n\nDrag & drop a video file here\nor click Browse",
        m_placeholderCard
    );
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setMinimumSize(280, 200);
    placeholderLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    placeholderLabel->setStyleSheet(
        "border: 2px dashed #888888; border-radius: 8px; background-color: rgba(128, 128, 128, 0.08); padding: 12px; color: #555555;"
    );
    placeholderLayout->addWidget(placeholderLabel);
    leftLayout->addWidget(m_placeholderCard, 1);

    // Dual Thumbnails Container (shown once video is loaded)
    auto* thumbLayout = new QHBoxLayout(m_thumbnailsContainer);
    thumbLayout->setContentsMargins(0, 0, 0, 0);
    thumbLayout->setSpacing(10);

    // Start Thumbnail Box
    auto* startBox = new QWidget(m_thumbnailsContainer);
    auto* startBoxLayout = new QVBoxLayout(startBox);
    startBoxLayout->setContentsMargins(0, 0, 0, 0);
    startBoxLayout->setSpacing(4);

    m_startThumbnailLabel->setAlignment(Qt::AlignCenter);
    m_startThumbnailLabel->setMinimumSize(130, 110);
    m_startThumbnailLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_startThumbnailLabel->setStyleSheet(
        "border: 1px solid #aaaaaa; border-radius: 6px; background-color: rgba(0, 0, 0, 0.04);"
    );
    m_startCaptionLabel->setAlignment(Qt::AlignCenter);
    m_startCaptionLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #3b82f6;");
    startBoxLayout->addWidget(m_startThumbnailLabel, 1);
    startBoxLayout->addWidget(m_startCaptionLabel);

    // End Thumbnail Box
    auto* endBox = new QWidget(m_thumbnailsContainer);
    auto* endBoxLayout = new QVBoxLayout(endBox);
    endBoxLayout->setContentsMargins(0, 0, 0, 0);
    endBoxLayout->setSpacing(4);

    m_endThumbnailLabel->setAlignment(Qt::AlignCenter);
    m_endThumbnailLabel->setMinimumSize(130, 110);
    m_endThumbnailLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_endThumbnailLabel->setStyleSheet(
        "border: 1px solid #aaaaaa; border-radius: 6px; background-color: rgba(0, 0, 0, 0.04);"
    );
    m_endCaptionLabel->setAlignment(Qt::AlignCenter);
    m_endCaptionLabel->setStyleSheet("font-size: 11px; font-weight: bold; color: #3b82f6;");
    endBoxLayout->addWidget(m_endThumbnailLabel, 1);
    endBoxLayout->addWidget(m_endCaptionLabel);

    thumbLayout->addWidget(startBox, 1);
    thumbLayout->addWidget(endBox, 1);

    m_thumbnailsContainer->setVisible(false);
    leftLayout->addWidget(m_thumbnailsContainer, 1);

    // Metadata Banner
    m_metadataLabel->setAlignment(Qt::AlignCenter);
    m_metadataLabel->setStyleSheet(
        "color: #475569; font-size: 11px; padding: 4px 8px; background: rgba(0,0,0,0.03); border-radius: 4px;"
    );
    m_metadataLabel->setWordWrap(true);
    leftLayout->addWidget(m_metadataLabel);

    // Range Timeline Widget
    leftLayout->addWidget(m_timelineWidget);

    // ==================== RIGHT COLUMN: CONTROLS & ACTIONS ====================
    auto* rightWidget = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    // Video Input
    auto* videoLabel = new QLabel("Video File", this);
    videoLabel->setStyleSheet("font-weight: bold;");
    auto* videoLayout = new QHBoxLayout();
    videoLayout->addWidget(m_videoPath);
    videoLayout->addWidget(m_videoButton);
    rightLayout->addWidget(videoLabel);
    rightLayout->addLayout(videoLayout);

    // Time Selection Group
    auto* timeHeading = new QLabel("Time Range", this);
    timeHeading->setStyleSheet("font-weight: bold; margin-top: 4px;");
    rightLayout->addWidget(timeHeading);

    // Start Time Row
    m_startTimeEdit->setDisplayFormat("HH:mm:ss");
    m_startTimeEdit->setTime(QTime(0, 0, 0));
    auto* startRow = new QHBoxLayout();
    startRow->addWidget(new QLabel("Start:", this));
    startRow->addWidget(m_startTimeEdit, 1);
    startRow->addWidget(m_startFromBeginningButton);
    m_startFromBeginningButton->setToolTip("Reset start to 00:00:00");
    rightLayout->addLayout(startRow);

    // End Time Row
    m_endTimeEdit->setDisplayFormat("HH:mm:ss");
    m_endTimeEdit->setTime(QTime(0, 0, 0));
    auto* endRow = new QHBoxLayout();
    endRow->addWidget(new QLabel("End:  ", this));
    endRow->addWidget(m_endTimeEdit, 1);
    endRow->addWidget(m_endToEndButton);
    m_endToEndButton->setToolTip("Reset end to full video duration");
    rightLayout->addLayout(endRow);

    // Summary & Validation
    m_trimSummaryLabel->setStyleSheet("font-weight: 500; font-size: 12px; color: #1e293b;");
    rightLayout->addWidget(m_trimSummaryLabel);

    // Mode Toggle
    m_reencodeCheckBox->setToolTip("Use frame-accurate re-encoding instead of fast stream copy.");
    rightLayout->addWidget(m_reencodeCheckBox);

    // Output Location
    auto* outputLabel = new QLabel("Output Destination", this);
    outputLabel->setStyleSheet("font-weight: bold; margin-top: 4px;");
    auto* outputLayout = new QHBoxLayout();
    outputLayout->addWidget(m_outputPath);
    outputLayout->addWidget(m_outputButton);
    rightLayout->addWidget(outputLabel);
    rightLayout->addLayout(outputLayout);

    rightLayout->addStretch(1);

    // Actions & Progress
    auto* actionLayout = new QHBoxLayout();
    m_trimButton->setStyleSheet("padding: 6px 14px; font-weight: bold;");
    actionLayout->addWidget(m_trimButton);
    actionLayout->addWidget(m_cancelButton);
    m_cancelButton->setEnabled(false);

    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);

    rightLayout->addLayout(actionLayout);
    rightLayout->addWidget(m_progressBar);
    rightLayout->addWidget(m_statusLabel);

    rootLayout->addWidget(leftWidget, 1);
    rootLayout->addWidget(rightWidget, 1);

    // ==================== CONNECTIONS ====================
    connect(m_videoButton, &QPushButton::clicked,
            this, &VideoTrimWidget::chooseVideo);

    connect(m_outputButton, &QPushButton::clicked,
            this, &VideoTrimWidget::chooseOutput);

    connect(m_trimButton, &QPushButton::clicked,
            this, &VideoTrimWidget::trimVideo);

    connect(m_cancelButton, &QPushButton::clicked,
            this, &VideoTrimWidget::cancelTrim);

    connect(m_videoPath, &QLineEdit::textChanged,
            this, &VideoTrimWidget::onVideoPathChanged);

    connect(m_startTimeEdit, &QTimeEdit::timeChanged,
            this, &VideoTrimWidget::onStartTimeChanged);

    connect(m_endTimeEdit, &QTimeEdit::timeChanged,
            this, &VideoTrimWidget::onEndTimeChanged);

    connect(m_startFromBeginningButton, &QPushButton::clicked,
            this, &VideoTrimWidget::onResetStartToBeginning);

    connect(m_endToEndButton, &QPushButton::clicked,
            this, &VideoTrimWidget::onResetEndToTotal);

    connect(m_thumbnailTimer, &QTimer::timeout,
            this, &VideoTrimWidget::onThumbnailDebounceTimeout);

    connect(&m_mediaService, &MediaService::videoTrimStarted,
            this, &VideoTrimWidget::onVideoTrimStarted);

    connect(&m_mediaService, &MediaService::progressChanged,
            this, &VideoTrimWidget::onProgressChanged);

    connect(&m_mediaService, &MediaService::videoTrimCancelled,
            this, &VideoTrimWidget::onVideoTrimCancelled);

    connect(&m_mediaService, &MediaService::videoTrimFinished,
            this, &VideoTrimWidget::onVideoTrimFinished);

    updateValidationAndSummary();
}

void VideoTrimWidget::chooseVideo()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Choose Video",
        QString(),
        "Videos (*.mp4 *.mkv *.mov *.webm *.avi)"
    );

    if (!path.isEmpty()) {
        m_videoPath->setText(path);
    }
}

void VideoTrimWidget::chooseOutput()
{
    const QString path = QFileDialog::getSaveFileName(
        this,
        "Choose Output",
        m_outputPath->text().trimmed().isEmpty() ? "trimmed.mp4" : m_outputPath->text().trimmed(),
        "MP4 Video (*.mp4);;All Files (*.*)"
    );

    if (!path.isEmpty()) {
        m_outputPath->setText(path);
    }
}

void VideoTrimWidget::onVideoPathChanged(const QString& path)
{
    loadVideo(path.trimmed());
}

void VideoTrimWidget::loadVideo(const QString& path)
{
    if (path.isEmpty() || !QFile::exists(path)) {
        m_videoLoaded = false;
        m_currentMetadata = VideoMetadata();
        m_placeholderCard->setVisible(true);
        m_thumbnailsContainer->setVisible(false);
        m_metadataLabel->setText("No video loaded");
        m_timelineWidget->setRange(0, 0, 0);
        m_startPixmap = QPixmap();
        m_endPixmap = QPixmap();
        updateValidationAndSummary();
        return;
    }

    VideoMetadata meta;
    QString err;
    if (!m_mediaService.getVideoMetadata(path, meta, &err)) {
        m_videoLoaded = false;
        m_currentMetadata = VideoMetadata();
        m_placeholderCard->setVisible(true);
        m_thumbnailsContainer->setVisible(false);
        m_metadataLabel->setText("Could not probe video metadata");
        m_timelineWidget->setRange(0, 0, 0);
        updateValidationAndSummary();
        return;
    }

    m_currentMetadata = meta;
    m_videoLoaded = true;

    // Display metadata banner
    QString sizeStr;
    if (meta.fileSizeBytes < 1024 * 1024) {
        sizeStr = QString("%1 KB").arg(meta.fileSizeBytes / 1024.0, 0, 'f', 1);
    } else {
        sizeStr = QString("%1 MB").arg(meta.fileSizeBytes / (1024.0 * 1024.0), 0, 'f', 1);
    }

    const QString resStr = (meta.width > 0 && meta.height > 0)
        ? QString("%1×%2").arg(meta.width).arg(meta.height)
        : "Unknown res";

    const QString videoCodec = meta.videoCodec.isEmpty() ? "Unknown" : meta.videoCodec.toUpper();
    const QString audioCodec = meta.audioCodec.isEmpty() ? "None" : meta.audioCodec.toUpper();
    const QString durStr = formatDuration(meta.durationSeconds);

    m_metadataLabel->setText(
        QString("%1 · %2 / %3 · %4 · %5").arg(resStr, videoCodec, audioCodec, durStr, sizeStr)
    );

    // Setup TimeEdits range and values
    const QTime maxTime = secondsToQTime(meta.durationSeconds);
    m_startTimeEdit->setMaximumTime(maxTime);
    m_endTimeEdit->setMaximumTime(maxTime);

    m_startTimeEdit->setTime(QTime(0, 0, 0));
    m_endTimeEdit->setTime(maxTime);

    // Setup timeline
    m_timelineWidget->setRange(0.0, meta.durationSeconds, meta.durationSeconds);

    // Auto-suggest output path if empty or default
    const QFileInfo fileInfo(path);
    const QString suggestedOut = fileInfo.dir().filePath(
        QString("%1_trimmed.%2").arg(fileInfo.completeBaseName(), fileInfo.suffix())
    );
    m_outputPath->setText(suggestedOut);

    // Switch view to thumbnails
    m_placeholderCard->setVisible(false);
    m_thumbnailsContainer->setVisible(true);

    updateValidationAndSummary();
    requestThumbnailUpdate();
}

void VideoTrimWidget::onStartTimeChanged(const QTime& time)
{
    Q_UNUSED(time);
    const double startSec = qTimeToSeconds(m_startTimeEdit->time());
    const double endSec = qTimeToSeconds(m_endTimeEdit->time());

    m_timelineWidget->setTimeRange(startSec, endSec);
    updateValidationAndSummary();
    requestThumbnailUpdate();
}

void VideoTrimWidget::onEndTimeChanged(const QTime& time)
{
    Q_UNUSED(time);
    const double startSec = qTimeToSeconds(m_startTimeEdit->time());
    const double endSec = qTimeToSeconds(m_endTimeEdit->time());

    m_timelineWidget->setTimeRange(startSec, endSec);
    updateValidationAndSummary();
    requestThumbnailUpdate();
}

void VideoTrimWidget::onResetStartToBeginning()
{
    m_startTimeEdit->setTime(QTime(0, 0, 0));
}

void VideoTrimWidget::onResetEndToTotal()
{
    if (m_videoLoaded && m_currentMetadata.durationSeconds > 0.0) {
        m_endTimeEdit->setTime(secondsToQTime(m_currentMetadata.durationSeconds));
    }
}

void VideoTrimWidget::updateValidationAndSummary()
{
    if (!m_videoLoaded) {
        m_trimSummaryLabel->setText("No video loaded");
        m_trimSummaryLabel->setStyleSheet("color: #64748b;");
        m_trimButton->setEnabled(false);
        return;
    }

    const double startSec = qTimeToSeconds(m_startTimeEdit->time());
    const double endSec = qTimeToSeconds(m_endTimeEdit->time());

    if (endSec <= startSec) {
        m_trimSummaryLabel->setText("Invalid Range: End time must be after start time");
        m_trimSummaryLabel->setStyleSheet("color: #ef4444; font-weight: bold;");
        m_trimButton->setEnabled(false);
        return;
    }

    const double cutDuration = endSec - startSec;
    m_trimSummaryLabel->setText(
        QString("Selected: %1 (from %2 to %3)")
            .arg(formatDuration(cutDuration), formatDuration(startSec), formatDuration(endSec))
    );
    m_trimSummaryLabel->setStyleSheet("color: #0f172a; font-weight: 500;");
    m_trimButton->setEnabled(!m_mediaService.isProcessing());
}

void VideoTrimWidget::requestThumbnailUpdate()
{
    if (!m_videoLoaded) {
        return;
    }
    m_thumbnailTimer->start();
}

void VideoTrimWidget::onThumbnailDebounceTimeout()
{
    updateThumbnails();
}

void VideoTrimWidget::updateThumbnails()
{
    const QString path = m_videoPath->text().trimmed();
    if (path.isEmpty() || !QFile::exists(path) || !m_videoLoaded) {
        return;
    }

    const double startSec = qTimeToSeconds(m_startTimeEdit->time());
    const double endSec = qTimeToSeconds(m_endTimeEdit->time());

    QPixmap startPix;
    if (m_mediaService.extractThumbnail(path, startSec, startPix)) {
        m_startPixmap = startPix;
    }

    QPixmap endPix;
    if (m_mediaService.extractThumbnail(path, endSec, endPix)) {
        m_endPixmap = endPix;
    }

    m_startCaptionLabel->setText(QString("Start: %1").arg(formatDuration(startSec)));
    m_endCaptionLabel->setText(QString("End: %1").arg(formatDuration(endSec)));

    renderThumbnails();
}

void VideoTrimWidget::renderThumbnails()
{
    if (!m_startPixmap.isNull()) {
        const QSize targetSize = m_startThumbnailLabel->size() - QSize(8, 8);
        if (targetSize.width() > 0 && targetSize.height() > 0) {
            m_startThumbnailLabel->setPixmap(
                m_startPixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            );
        }
    } else {
        m_startThumbnailLabel->setText("No preview");
    }

    if (!m_endPixmap.isNull()) {
        const QSize targetSize = m_endThumbnailLabel->size() - QSize(8, 8);
        if (targetSize.width() > 0 && targetSize.height() > 0) {
            m_endThumbnailLabel->setPixmap(
                m_endPixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            );
        }
    } else {
        m_endThumbnailLabel->setText("No preview");
    }
}

void VideoTrimWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    renderThumbnails();
}

void VideoTrimWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (m_mediaService.isProcessing()) {
        event->ignore();
        return;
    }

    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl& url : urls) {
            const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
            if (suffix == "mp4" || suffix == "mkv" || suffix == "mov" || suffix == "webm" || suffix == "avi") {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void VideoTrimWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (m_mediaService.isProcessing()) {
        event->ignore();
        return;
    }

    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void VideoTrimWidget::dropEvent(QDropEvent* event)
{
    if (m_mediaService.isProcessing()) {
        event->ignore();
        return;
    }

    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl& url : urls) {
            const QString localPath = url.toLocalFile();
            if (localPath.isEmpty() || !QFile::exists(localPath)) {
                continue;
            }
            const QString suffix = QFileInfo(localPath).suffix().toLower();
            if (suffix == "mp4" || suffix == "mkv" || suffix == "mov" || suffix == "webm" || suffix == "avi") {
                m_videoPath->setText(localPath);
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void VideoTrimWidget::trimVideo()
{
    const QString inputPath = m_videoPath->text().trimmed();
    const QString outputPath = m_outputPath->text().trimmed();

    if (inputPath.isEmpty() || !QFile::exists(inputPath)) {
        QMessageBox::warning(this, "Missing Input", "Please select a valid input video file.");
        return;
    }

    if (outputPath.isEmpty()) {
        QMessageBox::warning(this, "Missing Output", "Please choose an output destination.");
        return;
    }

    const QFileInfo outInfo(outputPath);
    if (!outInfo.dir().exists()) {
        QMessageBox::warning(this, "Invalid Output Location", "The destination directory does not exist.");
        return;
    }

    const QString canInput = QFileInfo(inputPath).canonicalFilePath();
    const QString canOutput = outInfo.canonicalFilePath();
    if (!canOutput.isEmpty() && canOutput == canInput) {
        QMessageBox::critical(this, "Invalid Output Path", "Output path cannot overwrite the input video file.");
        return;
    }

    const double startSec = qTimeToSeconds(m_startTimeEdit->time());
    const double endSec = qTimeToSeconds(m_endTimeEdit->time());

    if (endSec <= startSec) {
        QMessageBox::warning(this, "Invalid Time Range", "End time must be greater than start time.");
        return;
    }

    const bool accurateReencode = m_reencodeCheckBox->isChecked();

    QString errorMsg;
    const bool started = m_mediaService.trimVideo(
        inputPath,
        startSec,
        endSec,
        outputPath,
        accurateReencode,
        &errorMsg
    );

    if (!started) {
        m_statusLabel->setText("Failed to start trimming.");
        QMessageBox::critical(this, "Trimming Failed", errorMsg);
    }
}

void VideoTrimWidget::cancelTrim()
{
    m_statusLabel->setText("Cancelling video trimming...");
    m_cancelButton->setEnabled(false);
    m_mediaService.cancel();
}

void VideoTrimWidget::setUiBusy(bool busy)
{
    m_trimButton->setEnabled(!busy);
    m_cancelButton->setEnabled(busy);
    m_videoButton->setEnabled(!busy);
    m_videoPath->setEnabled(!busy);
    m_startTimeEdit->setEnabled(!busy);
    m_startFromBeginningButton->setEnabled(!busy);
    m_endTimeEdit->setEnabled(!busy);
    m_endToEndButton->setEnabled(!busy);
    m_reencodeCheckBox->setEnabled(!busy);
    m_outputPath->setEnabled(!busy);
    m_outputButton->setEnabled(!busy);

    if (busy) {
        m_progressBar->setValue(0);
    }
}

void VideoTrimWidget::onVideoTrimStarted()
{
    setUiBusy(true);
    m_statusLabel->setText("Trimming video...");
}

void VideoTrimWidget::onProgressChanged(int percentage)
{
    m_progressBar->setValue(percentage);
}

void VideoTrimWidget::onVideoTrimCancelled()
{
    setUiBusy(false);
    m_progressBar->setValue(0);
    m_statusLabel->setText("Video trimming cancelled.");

    QMessageBox::information(this, "Picture This", "Video trimming was cancelled.");
}

void VideoTrimWidget::onVideoTrimFinished(bool success, const QString& errorMessage)
{
    setUiBusy(false);

    if (success) {
        m_progressBar->setValue(100);
        m_statusLabel->setText("Video trimmed successfully.");
        QMessageBox::information(this, "Picture This", "The video was trimmed successfully.");
    } else {
        m_progressBar->setValue(0);
        m_statusLabel->setText("Failed to trim video.");
        QMessageBox::critical(this, "Trimming Failed", errorMessage);
    }
}

QTime VideoTrimWidget::secondsToQTime(double seconds)
{
    const int totalSec = std::max(0, static_cast<int>(seconds + 0.5));
    const int hrs = std::min(23, totalSec / 3600);
    const int mins = (totalSec % 3600) / 60;
    const int secs = totalSec % 60;
    return QTime(hrs, mins, secs);
}

double VideoTrimWidget::qTimeToSeconds(const QTime& time)
{
    return time.hour() * 3600.0 + time.minute() * 60.0 + time.second();
}

QString VideoTrimWidget::formatDuration(double seconds)
{
    const int totalSec = std::max(0, static_cast<int>(seconds + 0.5));
    const int hrs = totalSec / 3600;
    const int mins = (totalSec % 3600) / 60;
    const int secs = totalSec % 60;

    if (hrs > 0) {
        return QString::asprintf("%02d:%02d:%02d", hrs, mins, secs);
    }
    return QString::asprintf("%02d:%02d", mins, secs);
}
