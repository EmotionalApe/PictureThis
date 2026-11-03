#include "VideoCompressWidget.h"
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
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QUrl>
#include <QVBoxLayout>

VideoCompressWidget::VideoCompressWidget(
    MediaService& mediaService,
    QWidget* parent)
    : QWidget(parent),
      m_mediaService(mediaService),
      m_placeholderCard(new QWidget(this)),
      m_previewContainer(new QWidget(this)),
      m_thumbnailLabel(new QLabel(this)),
      m_sourceMetadataLabel(new QLabel("No video loaded", this)),
      m_compressionSummaryLabel(new QLabel(this)),
      m_advisoryLabel(new QLabel(this)),
      m_videoPath(new QLineEdit(this)),
      m_videoButton(new QPushButton("Browse...", this)),
      m_preset75Btn(new QPushButton("75% (Light)", this)),
      m_preset50Btn(new QPushButton("50% (Half)", this)),
      m_preset25Btn(new QPushButton("25% (Small)", this)),
      m_preset10Btn(new QPushButton("10% (Mini)", this)),
      m_percentageSlider(new QSlider(Qt::Horizontal, this)),
      m_percentageSpinBox(new QSpinBox(this)),
      m_targetSizeBadge(new QLabel("Target: --", this)),
      m_resolutionComboBox(new QComboBox(this)),
      m_audioQualityComboBox(new QComboBox(this)),
      m_twoPassCheckBox(new QCheckBox("Two-pass encoding (Optimal quality & strict file size)", this)),
      m_outputPath(new QLineEdit(this)),
      m_outputButton(new QPushButton("Browse...", this)),
      m_compressButton(new QPushButton("Compress Video", this)),
      m_cancelButton(new QPushButton("Cancel", this)),
      m_progressBar(new QProgressBar(this)),
      m_statusLabel(new QLabel("Ready", this))
{
    setAcceptDrops(true);

    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(20);

    // ==================== LEFT COLUMN: PREVIEWS & SUMMARY ====================
    auto* leftWidget = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    auto* previewHeading = new QLabel("Video Preview & Compression Summary", this);
    previewHeading->setStyleSheet("font-weight: bold; font-size: 13px; color: palette(window-text);");
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

    // Preview Container (shown once video is loaded)
    auto* containerLayout = new QVBoxLayout(m_previewContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(8);

    m_thumbnailLabel->setAlignment(Qt::AlignCenter);
    m_thumbnailLabel->setMinimumSize(260, 160);
    m_thumbnailLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_thumbnailLabel->setStyleSheet(
        "border: 1px solid palette(mid); border-radius: 6px; background-color: #111111;"
    );
    containerLayout->addWidget(m_thumbnailLabel, 1);

    // Source Metadata Banner
    m_sourceMetadataLabel->setStyleSheet(
        "background-color: rgba(128, 128, 128, 0.1); border: 1px solid palette(mid); border-radius: 6px; padding: 6px 10px; font-size: 11px;"
    );
    containerLayout->addWidget(m_sourceMetadataLabel);

    // Compression Summary Card
    m_compressionSummaryLabel->setStyleSheet(
        "background-color: rgba(13, 148, 136, 0.08); border: 1px solid #0d9488; border-radius: 6px; padding: 8px 10px; font-size: 11px;"
    );
    containerLayout->addWidget(m_compressionSummaryLabel);

    // Low Bitrate Advisory Banner (hidden by default)
    m_advisoryLabel->setStyleSheet(
        "background-color: rgba(234, 179, 8, 0.15); border: 1px solid #ca8a04; border-radius: 6px; padding: 6px 10px; font-size: 11px; color: #a16207;"
    );
    m_advisoryLabel->setWordWrap(true);
    m_advisoryLabel->hide();
    containerLayout->addWidget(m_advisoryLabel);

    m_previewContainer->hide();
    leftLayout->addWidget(m_previewContainer, 1);

    // ==================== RIGHT COLUMN: SETTINGS & CONTROLS ====================
    auto* rightWidget = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(12);

    auto* controlsHeading = new QLabel("Compression Settings", this);
    controlsHeading->setStyleSheet("font-weight: bold; font-size: 13px; color: palette(window-text);");
    rightLayout->addWidget(controlsHeading);

    // 1. Video file input
    auto* videoSection = new QWidget(this);
    auto* videoLayout = new QVBoxLayout(videoSection);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->setSpacing(4);
    auto* videoLabel = new QLabel("Source Video File:", this);
    videoLabel->setStyleSheet("font-weight: 500; font-size: 11px;");
    auto* videoRow = new QHBoxLayout();
    videoRow->setSpacing(6);
    m_videoPath->setPlaceholderText("Select a video file (*.mp4, *.mkv, *.mov, *.webm, *.avi)...");
    videoRow->addWidget(m_videoPath, 1);
    videoRow->addWidget(m_videoButton);
    videoLayout->addWidget(videoLabel);
    videoLayout->addLayout(videoRow);
    rightLayout->addWidget(videoSection);

    // 2. Target File Size (Percentage Controls)
    auto* targetSection = new QWidget(this);
    auto* targetLayout = new QVBoxLayout(targetSection);
    targetLayout->setContentsMargins(0, 0, 0, 0);
    targetLayout->setSpacing(6);

    auto* targetHeaderRow = new QHBoxLayout();
    auto* targetLabel = new QLabel("Target Size (Percentage of Original):", this);
    targetLabel->setStyleSheet("font-weight: 500; font-size: 11px;");
    targetHeaderRow->addWidget(targetLabel);
    targetHeaderRow->addStretch();
    m_targetSizeBadge->setStyleSheet(
        "font-weight: bold; font-size: 11px; color: #0d9488; background-color: rgba(13, 148, 136, 0.12); border-radius: 4px; padding: 2px 6px;"
    );
    targetHeaderRow->addWidget(m_targetSizeBadge);
    targetLayout->addLayout(targetHeaderRow);

    // Preset Buttons
    auto* presetRow = new QHBoxLayout();
    presetRow->setSpacing(6);
    m_preset75Btn->setCursor(Qt::PointingHandCursor);
    m_preset50Btn->setCursor(Qt::PointingHandCursor);
    m_preset25Btn->setCursor(Qt::PointingHandCursor);
    m_preset10Btn->setCursor(Qt::PointingHandCursor);

    presetRow->addWidget(m_preset75Btn);
    presetRow->addWidget(m_preset50Btn);
    presetRow->addWidget(m_preset25Btn);
    presetRow->addWidget(m_preset10Btn);
    targetLayout->addLayout(presetRow);

    // Slider + Spinbox Row
    auto* sliderRow = new QHBoxLayout();
    sliderRow->setSpacing(10);
    m_percentageSlider->setRange(5, 95);
    m_percentageSlider->setValue(50);
    m_percentageSlider->setSingleStep(1);
    m_percentageSlider->setPageStep(5);

    m_percentageSpinBox->setRange(5, 95);
    m_percentageSpinBox->setValue(50);
    m_percentageSpinBox->setSuffix("%");
    m_percentageSpinBox->setAlignment(Qt::AlignCenter);
    m_percentageSpinBox->setMinimumWidth(88);

    sliderRow->addWidget(m_percentageSlider, 1);
    sliderRow->addWidget(m_percentageSpinBox);
    targetLayout->addLayout(sliderRow);
    rightLayout->addWidget(targetSection);

    // 3. Advanced Encoding Settings
    auto* settingsGrid = new QGridLayout();
    settingsGrid->setSpacing(8);

    auto* resLabel = new QLabel("Resolution:", this);
    resLabel->setStyleSheet("font-size: 11px; font-weight: 500;");
    m_resolutionComboBox->addItem("Auto (Adaptive to bitrate)", 0);
    m_resolutionComboBox->addItem("Keep Original", -1);
    m_resolutionComboBox->addItem("1080p (Full HD)", 1080);
    m_resolutionComboBox->addItem("720p (HD)", 720);
    m_resolutionComboBox->addItem("480p (SD)", 480);
    m_resolutionComboBox->addItem("360p (Low)", 360);
    settingsGrid->addWidget(resLabel, 0, 0);
    settingsGrid->addWidget(m_resolutionComboBox, 0, 1);

    auto* audioLabel = new QLabel("Audio Quality:", this);
    audioLabel->setStyleSheet("font-size: 11px; font-weight: 500;");
    m_audioQualityComboBox->addItem("Auto (Recommended)", 0);
    m_audioQualityComboBox->addItem("128 kbps (Standard)", 128);
    m_audioQualityComboBox->addItem("96 kbps (Voice/Speech)", 96);
    m_audioQualityComboBox->addItem("64 kbps (Low bitrate)", 64);
    settingsGrid->addWidget(audioLabel, 1, 0);
    settingsGrid->addWidget(m_audioQualityComboBox, 1, 1);

    rightLayout->addLayout(settingsGrid);

    m_twoPassCheckBox->setChecked(true);
    m_twoPassCheckBox->setStyleSheet("font-size: 11px; font-weight: 500;");
    rightLayout->addWidget(m_twoPassCheckBox);

    // 4. Output Path
    auto* outputSection = new QWidget(this);
    auto* outputLayout = new QVBoxLayout(outputSection);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(4);
    auto* outputLabel = new QLabel("Output Video Path:", this);
    outputLabel->setStyleSheet("font-weight: 500; font-size: 11px;");
    auto* outputRow = new QHBoxLayout();
    outputRow->setSpacing(6);
    m_outputPath->setPlaceholderText("Choose output MP4 destination...");
    outputRow->addWidget(m_outputPath, 1);
    outputRow->addWidget(m_outputButton);
    outputLayout->addWidget(outputLabel);
    outputLayout->addLayout(outputRow);
    rightLayout->addWidget(outputSection);

    // 5. Actions, Progress & Status
    rightLayout->addStretch(1);

    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    rightLayout->addWidget(m_progressBar);

    auto* actionRow = new QHBoxLayout();
    actionRow->setSpacing(8);
    m_compressButton->setCursor(Qt::PointingHandCursor);
    m_compressButton->setStyleSheet(
        "QPushButton { background-color: #0d9488; color: white; font-weight: bold; border-radius: 4px; padding: 7px 18px; font-size: 12px; }"
        "QPushButton:hover { background-color: #0f766e; }"
        "QPushButton:disabled { background-color: rgba(128, 128, 128, 0.3); color: rgba(255, 255, 255, 0.4); }"
    );
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    m_cancelButton->setEnabled(false);
    m_cancelButton->setStyleSheet("padding: 7px 14px; font-size: 12px;");

    actionRow->addWidget(m_compressButton, 1);
    actionRow->addWidget(m_cancelButton);
    rightLayout->addLayout(actionRow);

    m_statusLabel->setStyleSheet("font-size: 11px; color: palette(mid);");
    rightLayout->addWidget(m_statusLabel);

    rootLayout->addWidget(leftWidget, 5);
    rootLayout->addWidget(rightWidget, 6);

    // ==================== CONNECTIONS ====================
    connect(m_videoButton, &QPushButton::clicked, this, &VideoCompressWidget::chooseVideo);
    connect(m_outputButton, &QPushButton::clicked, this, &VideoCompressWidget::chooseOutput);
    connect(m_videoPath, &QLineEdit::textChanged, this, &VideoCompressWidget::onVideoPathChanged);

    connect(m_preset75Btn, &QPushButton::clicked, this, [this]() { onPresetButtonClicked(75); });
    connect(m_preset50Btn, &QPushButton::clicked, this, [this]() { onPresetButtonClicked(50); });
    connect(m_preset25Btn, &QPushButton::clicked, this, [this]() { onPresetButtonClicked(25); });
    connect(m_preset10Btn, &QPushButton::clicked, this, [this]() { onPresetButtonClicked(10); });

    connect(m_percentageSlider, &QSlider::valueChanged, this, &VideoCompressWidget::onPercentageSliderChanged);
    connect(m_percentageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &VideoCompressWidget::onPercentageSpinBoxChanged);

    connect(m_resolutionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoCompressWidget::onResolutionChanged);
    connect(m_audioQualityComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoCompressWidget::onAudioQualityChanged);

    connect(m_compressButton, &QPushButton::clicked, this, &VideoCompressWidget::compressVideo);
    connect(m_cancelButton, &QPushButton::clicked, this, &VideoCompressWidget::cancelCompression);

    connect(&m_mediaService, &MediaService::videoCompressStarted, this, &VideoCompressWidget::onVideoCompressStarted);
    connect(&m_mediaService, &MediaService::progressChanged, this, &VideoCompressWidget::onProgressChanged);
    connect(&m_mediaService, &MediaService::compressStatusChanged, this, &VideoCompressWidget::onCompressStatusChanged);
    connect(&m_mediaService, &MediaService::videoCompressCancelled, this, &VideoCompressWidget::onVideoCompressCancelled);
    connect(&m_mediaService, &MediaService::videoCompressFinished, this, &VideoCompressWidget::onVideoCompressFinished);

    updateCompressionCalculations();
}

void VideoCompressWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            event->acceptProposedAction();
        }
    }
}

void VideoCompressWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void VideoCompressWidget::dropEvent(QDropEvent* event)
{
    const auto urls = event->mimeData()->urls();
    if (!urls.isEmpty() && urls.first().isLocalFile()) {
        const QString path = urls.first().toLocalFile();
        m_videoPath->setText(path);
        event->acceptProposedAction();
    }
}

void VideoCompressWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    renderThumbnail();
}

void VideoCompressWidget::chooseVideo()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select Video to Compress",
        QString(),
        "Video Files (*.mp4 *.mkv *.mov *.webm *.avi);;All Files (*.*)"
    );

    if (!filePath.isEmpty()) {
        m_videoPath->setText(filePath);
    }
}

void VideoCompressWidget::chooseOutput()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        "Choose Output Video File",
        m_outputPath->text(),
        "MP4 Video (*.mp4)"
    );

    if (!filePath.isEmpty()) {
        m_outputPath->setText(filePath);
    }
}

void VideoCompressWidget::onVideoPathChanged(const QString& path)
{
    loadVideo(path);
}

void VideoCompressWidget::loadVideo(const QString& path)
{
    m_videoLoaded = false;
    m_sourceThumbnailPixmap = QPixmap();
    m_thumbnailLabel->clear();

    if (path.isEmpty() || !QFile::exists(path)) {
        m_currentMetadata = VideoMetadata();
        m_placeholderCard->show();
        m_previewContainer->hide();
        m_sourceMetadataLabel->setText("No video loaded");
        updateCompressionCalculations();
        return;
    }

    QString probeError;
    if (!m_mediaService.getVideoMetadata(path, m_currentMetadata, &probeError)) {
        m_placeholderCard->show();
        m_previewContainer->hide();
        m_statusLabel->setText(QString("Probe error: %1").arg(probeError));
        updateCompressionCalculations();
        return;
    }

    m_videoLoaded = true;
    m_placeholderCard->hide();
    m_previewContainer->show();

    // Source Metadata Banner
    const qint64 origSize = (m_currentMetadata.fileSizeBytes > 0)
        ? m_currentMetadata.fileSizeBytes
        : QFileInfo(path).size();
    m_currentMetadata.fileSizeBytes = origSize;

    double origBitrateKbps = 0.0;
    if (m_currentMetadata.durationSeconds > 0.0) {
        origBitrateKbps = (origSize * 8.0) / (m_currentMetadata.durationSeconds * 1000.0);
    }

    m_sourceMetadataLabel->setText(
        QString("<b>Duration:</b> %1 &nbsp;|&nbsp; <b>Resolution:</b> %2x%3 &nbsp;|&nbsp; <b>Size:</b> %4<br>"
                "<b>Video:</b> %5 (%6 kbps) &nbsp;|&nbsp; <b>Audio:</b> %7")
            .arg(formatDuration(m_currentMetadata.durationSeconds))
            .arg(m_currentMetadata.width)
            .arg(m_currentMetadata.height)
            .arg(formatFileSize(origSize))
            .arg(m_currentMetadata.videoCodec.isEmpty() ? "unknown" : m_currentMetadata.videoCodec)
            .arg(static_cast<int>(origBitrateKbps))
            .arg(m_currentMetadata.audioCodec.isEmpty() ? "none" : m_currentMetadata.audioCodec)
    );

    // Auto-generate output filename if blank or matching previous default
    const QFileInfo fileInfo(path);
    const QString defaultOutput = fileInfo.dir().filePath(
        fileInfo.completeBaseName() + "_compressed.mp4"
    );
    m_outputPath->setText(defaultOutput);

    // Extract thumbnail at ~10% timestamp
    const double thumbTimestamp = std::min(1.0, m_currentMetadata.durationSeconds * 0.1);
    QPixmap pix;
    if (m_mediaService.extractThumbnail(path, thumbTimestamp, pix)) {
        m_sourceThumbnailPixmap = pix;
        renderThumbnail();
    }

    updateCompressionCalculations();
}

void VideoCompressWidget::renderThumbnail()
{
    if (m_sourceThumbnailPixmap.isNull()) {
        m_thumbnailLabel->clear();
        return;
    }

    const QSize targetSize = m_thumbnailLabel->size();
    if (targetSize.width() <= 10 || targetSize.height() <= 10) {
        return;
    }

    m_thumbnailLabel->setPixmap(
        m_sourceThumbnailPixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
}

void VideoCompressWidget::onPercentageSliderChanged(int value)
{
    if (m_percentageSpinBox->value() != value) {
        m_percentageSpinBox->blockSignals(true);
        m_percentageSpinBox->setValue(value);
        m_percentageSpinBox->blockSignals(false);
    }
    updateCompressionCalculations();
}

void VideoCompressWidget::onPercentageSpinBoxChanged(int value)
{
    if (m_percentageSlider->value() != value) {
        m_percentageSlider->blockSignals(true);
        m_percentageSlider->setValue(value);
        m_percentageSlider->blockSignals(false);
    }
    updateCompressionCalculations();
}

void VideoCompressWidget::onPresetButtonClicked(int percentage)
{
    m_percentageSlider->setValue(percentage);
}

void VideoCompressWidget::onResolutionChanged(int)
{
    updateCompressionCalculations();
}

void VideoCompressWidget::onAudioQualityChanged(int)
{
    updateCompressionCalculations();
}

void VideoCompressWidget::updateCompressionCalculations()
{
    const int pct = m_percentageSlider->value();

    // Update preset button active visuals
    const QString activeStyle = "font-weight: bold; background-color: rgba(13, 148, 136, 0.2); border: 1px solid #0d9488; color: #0d9488;";
    const QString inactiveStyle = "";
    m_preset75Btn->setStyleSheet(pct == 75 ? activeStyle : inactiveStyle);
    m_preset50Btn->setStyleSheet(pct == 50 ? activeStyle : inactiveStyle);
    m_preset25Btn->setStyleSheet(pct == 25 ? activeStyle : inactiveStyle);
    m_preset10Btn->setStyleSheet(pct == 10 ? activeStyle : inactiveStyle);

    if (!m_videoLoaded || m_currentMetadata.durationSeconds <= 0.0) {
        m_targetSizeBadge->setText("Target: --");
        m_compressionSummaryLabel->setText("Load a video to see compression metrics.");
        m_advisoryLabel->hide();
        return;
    }

    const qint64 origSize = m_currentMetadata.fileSizeBytes;
    const double targetSizeBytes = static_cast<double>(origSize) * (static_cast<double>(pct) / 100.0);
    const double savedBytes = std::max(0.0, static_cast<double>(origSize) - targetSizeBytes);

    m_targetSizeBadge->setText(QString("Target: %1").arg(formatFileSize(static_cast<qint64>(targetSizeBytes))));

    // Target Bitrate Derivation
    const double targetBudgetBits = targetSizeBytes * 0.97 * 8.0; // 3% container safety margin
    const double totalBitrateKbps = targetBudgetBits / (m_currentMetadata.durationSeconds * 1000.0);

    const bool hasAudio = !m_currentMetadata.audioCodec.isEmpty();
    int audioBitrateKbps = 0;
    if (hasAudio) {
        const int selectedAudio = m_audioQualityComboBox->currentData().toInt();
        if (selectedAudio > 0) {
            audioBitrateKbps = selectedAudio;
        } else {
            if (totalBitrateKbps < 300.0) {
                audioBitrateKbps = 64;
            } else if (totalBitrateKbps < 800.0) {
                audioBitrateKbps = 96;
            } else {
                audioBitrateKbps = 128;
            }
        }
        if (audioBitrateKbps >= totalBitrateKbps * 0.5) {
            audioBitrateKbps = std::max(32, static_cast<int>(totalBitrateKbps * 0.25));
        }
    }

    int videoBitrateKbps = static_cast<int>(totalBitrateKbps - audioBitrateKbps);
    videoBitrateKbps = std::max(videoBitrateKbps, 32);

    // Auto resolution suggestion
    QString resNotice;
    const int chosenHeight = m_resolutionComboBox->currentData().toInt();
    if (chosenHeight == 0) { // Auto
        if (videoBitrateKbps < 250) {
            resNotice = " (Auto will downscale to 360p)";
        } else if (videoBitrateKbps < 600) {
            resNotice = " (Auto will downscale to 480p)";
        } else if (videoBitrateKbps < 1200 && m_currentMetadata.height > 720) {
            resNotice = " (Auto will downscale to 720p)";
        }
    }

    m_compressionSummaryLabel->setText(
        QString("<b>Target Size:</b> %1 (%2% of original)<br>"
                "<b>Space Saved:</b> %3 (-%4%)<br>"
                "<b>Target Video Bitrate:</b> ~%5 kbps%6<br>"
                "<b>Allocated Audio Bitrate:</b> %7 kbps")
            .arg(formatFileSize(static_cast<qint64>(targetSizeBytes)))
            .arg(pct)
            .arg(formatFileSize(static_cast<qint64>(savedBytes)))
            .arg(100 - pct)
            .arg(videoBitrateKbps)
            .arg(resNotice)
            .arg(hasAudio ? QString::number(audioBitrateKbps) : "None")
    );

    // Advisory warning for very low bitrates
    if (videoBitrateKbps < 150) {
        m_advisoryLabel->setText(
            "<b>Warning: Target bitrate is extremely low (<150 kbps).</b><br>"
            "Video quality will degrade noticeably. Recommend downscaling to 360p or 480p."
        );
        m_advisoryLabel->show();
    } else if (videoBitrateKbps < 350 && chosenHeight == -1 && m_currentMetadata.height >= 1080) {
        m_advisoryLabel->setText(
            "<b>Notice:</b> Keeping 1080p+ at <350 kbps may cause blockiness. Downscaling to 720p or 480p is recommended."
        );
        m_advisoryLabel->show();
    } else {
        m_advisoryLabel->hide();
    }
}

void VideoCompressWidget::compressVideo()
{
    const QString input = m_videoPath->text().trimmed();
    const QString output = m_outputPath->text().trimmed();

    if (input.isEmpty() || !QFile::exists(input)) {
        QMessageBox::warning(this, "Missing Input", "Please select a valid input video file.");
        return;
    }

    if (output.isEmpty()) {
        QMessageBox::warning(this, "Missing Output", "Please specify an output file destination.");
        return;
    }

    if (QFileInfo(input).canonicalFilePath() == QFileInfo(output).canonicalFilePath()) {
        QMessageBox::warning(this, "Invalid Output", "Output destination cannot be the same as the input file.");
        return;
    }

    const int pct = m_percentageSlider->value();
    const qint64 origSize = m_currentMetadata.fileSizeBytes;
    const double targetSizeBytes = static_cast<double>(origSize) * (static_cast<double>(pct) / 100.0);

    VideoCompressOptions options;
    options.targetSizeBytes = targetSizeBytes;
    options.twoPass = m_twoPassCheckBox->isChecked();
    options.audioBitrateKbps = m_audioQualityComboBox->currentData().toInt();

    const int chosenHeight = m_resolutionComboBox->currentData().toInt();
    if (chosenHeight == 0) { // Auto
        const double targetBudgetBits = targetSizeBytes * 0.97 * 8.0;
        const double totalBitrateKbps = targetBudgetBits / (m_currentMetadata.durationSeconds * 1000.0);
        if (totalBitrateKbps < 300) {
            options.targetHeight = std::min(m_currentMetadata.height, 360);
        } else if (totalBitrateKbps < 700) {
            options.targetHeight = std::min(m_currentMetadata.height, 480);
        } else if (totalBitrateKbps < 1400 && m_currentMetadata.height > 720) {
            options.targetHeight = 720;
        } else {
            options.targetHeight = 0; // original
        }
    } else if (chosenHeight == -1) { // Original
        options.targetHeight = 0;
    } else {
        options.targetHeight = chosenHeight;
    }

    QString errorMsg;
    if (!m_mediaService.compressVideo(input, output, options, &errorMsg)) {
        QMessageBox::critical(this, "Compression Failed", QString("Could not start compression:\n%1").arg(errorMsg));
    }
}

void VideoCompressWidget::cancelCompression()
{
    m_cancelButton->setEnabled(false);
    m_statusLabel->setText("Cancelling compression...");
    m_mediaService.cancel();
}

void VideoCompressWidget::onVideoCompressStarted()
{
    setUiBusy(true);
    m_progressBar->setValue(0);
    m_statusLabel->setText("Starting compression...");
}

void VideoCompressWidget::onProgressChanged(int percentage)
{
    m_progressBar->setValue(percentage);
}

void VideoCompressWidget::onCompressStatusChanged(const QString& statusText)
{
    m_statusLabel->setText(statusText);
}

void VideoCompressWidget::onVideoCompressCancelled()
{
    setUiBusy(false);
    m_progressBar->setValue(0);
    m_statusLabel->setText("Compression cancelled.");
    QMessageBox::information(this, "Compression Cancelled", "The video compression operation was cancelled.");
}

void VideoCompressWidget::onVideoCompressFinished(bool success, const QString& errorMessage)
{
    setUiBusy(false);

    if (success) {
        m_progressBar->setValue(100);
        const qint64 finalSize = QFileInfo(m_outputPath->text()).size();
        m_statusLabel->setText(QString("Compression complete! Final size: %1").arg(formatFileSize(finalSize)));
        QMessageBox::information(
            this,
            "Compression Complete",
            QString("Video successfully compressed!\n\nDestination: %1\nFinal Size: %2")
                .arg(m_outputPath->text())
                .arg(formatFileSize(finalSize))
        );
    } else {
        m_progressBar->setValue(0);
        m_statusLabel->setText("Compression failed.");
        QMessageBox::critical(
            this,
            "Compression Failed",
            QString("An error occurred during video compression:\n%1").arg(errorMessage)
        );
    }
}

void VideoCompressWidget::setUiBusy(bool busy)
{
    m_videoButton->setEnabled(!busy);
    m_videoPath->setEnabled(!busy);
    m_preset75Btn->setEnabled(!busy);
    m_preset50Btn->setEnabled(!busy);
    m_preset25Btn->setEnabled(!busy);
    m_preset10Btn->setEnabled(!busy);
    m_percentageSlider->setEnabled(!busy);
    m_percentageSpinBox->setEnabled(!busy);
    m_resolutionComboBox->setEnabled(!busy);
    m_audioQualityComboBox->setEnabled(!busy);
    m_twoPassCheckBox->setEnabled(!busy);
    m_outputPath->setEnabled(!busy);
    m_outputButton->setEnabled(!busy);
    m_compressButton->setEnabled(!busy);
    m_cancelButton->setEnabled(busy);
}

QString VideoCompressWidget::formatFileSize(qint64 bytes)
{
    if (bytes <= 0) return "0 B";
    const double kb = static_cast<double>(bytes) / 1024.0;
    const double mb = kb / 1024.0;
    const double gb = mb / 1024.0;

    if (gb >= 1.0) {
        return QString("%1 GB").arg(gb, 0, 'f', 2);
    }
    if (mb >= 1.0) {
        return QString("%1 MB").arg(mb, 0, 'f', 1);
    }
    if (kb >= 1.0) {
        return QString("%1 KB").arg(kb, 0, 'f', 0);
    }
    return QString("%1 B").arg(bytes);
}

QString VideoCompressWidget::formatDuration(double seconds)
{
    if (seconds <= 0.0) return "00:00";
    const int totalSec = static_cast<int>(seconds);
    const int hrs = totalSec / 3600;
    const int mins = (totalSec % 3600) / 60;
    const int secs = totalSec % 60;

    if (hrs > 0) {
        return QString("%1:%2:%3")
            .arg(hrs, 2, 10, QChar('0'))
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}
