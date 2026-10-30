#include "VideoCombineWidget.h"
#include "../media/MediaService.h"

#include <algorithm>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QCheckBox>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

VideoCombineWidget::VideoCombineWidget(
    MediaService& mediaService,
    QWidget* parent)
    : QWidget(parent),
      m_mediaService(mediaService),
      m_placeholderCard(new QWidget(this)),
      m_placeholderAddButton(new QPushButton("+ Add Videos", this)),
      m_clipListWidget(new QListWidget(this)),
      m_addVideosButton(new QPushButton("+ Add Videos", this)),
      m_moveUpButton(new QPushButton("▲", this)),
      m_moveDownButton(new QPushButton("▼", this)),
      m_removeButton(new QPushButton("✕", this)),
      m_clearAllButton(new QPushButton("Clear All", this)),
      m_bannerContainer(new QWidget(this)),
      m_summaryLabel(new QLabel(this)),
      m_compatibilityLabel(new QLabel(this)),
      m_forceReencodeCheckBox(new QCheckBox("Force re-encode (H.264 / AAC universal compatibility)", this)),
      m_outputPath(new QLineEdit(this)),
      m_outputButton(new QPushButton("Browse...", this)),
      m_combineButton(new QPushButton("Combine Videos", this)),
      m_cancelButton(new QPushButton("Cancel", this)),
      m_progressBar(new QProgressBar(this)),
      m_statusLabel(new QLabel("Ready", this))
{
    setAcceptDrops(true);

    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(20);

    // ==================== LEFT COLUMN: PREVIEW, QUEUE & TOOLBAR ====================
    auto* leftWidget = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    auto* queueHeaderLabel = new QLabel("Video Preview & Queue", this);
    queueHeaderLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: palette(window-text);");
    leftLayout->addWidget(queueHeaderLabel);

    // Placeholder Card (shown when no video is in queue, matching Tab 1 and Tab 2)
    auto* placeholderLayout = new QVBoxLayout(m_placeholderCard);
    placeholderLayout->setContentsMargins(20, 24, 20, 24);
    placeholderLayout->setAlignment(Qt::AlignCenter);
    placeholderLayout->setSpacing(14);

    auto* placeholderLabel = new QLabel(
        "No videos in queue\n\nDrag & drop video clips here\nor click below to select files:",
        m_placeholderCard
    );
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setStyleSheet("color: #555555; font-size: 12px;");

    m_placeholderAddButton->setStyleSheet("font-weight: bold; padding: 7px 18px;");

    placeholderLayout->addStretch(1);
    placeholderLayout->addWidget(placeholderLabel);
    placeholderLayout->addWidget(m_placeholderAddButton, 0, Qt::AlignCenter);
    placeholderLayout->addStretch(1);

    m_placeholderCard->setStyleSheet(
        "QWidget#placeholderCard {"
        "    border: 2px dashed #888888;"
        "    border-radius: 8px;"
        "    background-color: rgba(128, 128, 128, 0.08);"
        "}"
    );
    m_placeholderCard->setObjectName("placeholderCard");
    leftLayout->addWidget(m_placeholderCard, 1);

    // Clip Queue List (occupies full height of column when populated)
    m_clipListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_clipListWidget->setIconSize(QSize(56, 35));
    m_clipListWidget->setStyleSheet(
        "QListWidget {"
        "    border: 1px solid palette(mid);"
        "    border-radius: 6px;"
        "    background-color: palette(base);"
        "    color: palette(text);"
        "    padding: 2px;"
        "}"
        "QListWidget::item {"
        "    padding: 6px 8px;"
        "    border-bottom: 1px solid palette(alternate-base);"
        "    border-radius: 4px;"
        "    color: palette(text);"
        "}"
        "QListWidget::item:selected {"
        "    background-color: palette(highlight);"
        "    color: palette(highlighted-text);"
        "}"
        "QListWidget::item:hover:!selected {"
        "    background-color: rgba(128, 128, 128, 0.12);"
        "    color: palette(text);"
        "}"
    );
    m_clipListWidget->setVisible(false);
    leftLayout->addWidget(m_clipListWidget, 1);

    // Toolbar buttons (native look matching other tabs)
    auto* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(6);

    m_addVideosButton->setStyleSheet("font-weight: bold; padding: 5px 12px;");
    m_moveUpButton->setStyleSheet("padding: 5px 10px;");
    m_moveUpButton->setToolTip("Move Up");
    m_moveDownButton->setStyleSheet("padding: 5px 10px;");
    m_moveDownButton->setToolTip("Move Down");
    m_removeButton->setStyleSheet("padding: 5px 10px;");
    m_removeButton->setToolTip("Remove Selected");
    m_clearAllButton->setStyleSheet("padding: 5px 10px;");

    toolbarLayout->addWidget(m_addVideosButton);
    toolbarLayout->addWidget(m_moveUpButton);
    toolbarLayout->addWidget(m_moveDownButton);
    toolbarLayout->addWidget(m_removeButton);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(m_clearAllButton);
    leftLayout->addLayout(toolbarLayout);

    // ==================== RIGHT COLUMN: SETTINGS, OUTPUT & ACTIONS ====================
    auto* rightWidget = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(12);

    // Summary & Compatibility Banner Card
    m_bannerContainer->setStyleSheet(
        "QWidget#bannerContainer {"
        "    border: 1px solid palette(mid);"
        "    border-radius: 6px;"
        "    background-color: rgba(128, 128, 128, 0.08);"
        "}"
    );
    m_bannerContainer->setObjectName("bannerContainer");

    auto* bannerLayout = new QVBoxLayout(m_bannerContainer);
    bannerLayout->setContentsMargins(12, 10, 12, 10);
    bannerLayout->setSpacing(6);

    auto* bannerTitle = new QLabel("Format & Compatibility", m_bannerContainer);
    bannerTitle->setStyleSheet("font-size: 12px; font-weight: bold; color: palette(window-text);");
    bannerLayout->addWidget(bannerTitle);

    m_summaryLabel->setStyleSheet("font-size: 11px; font-weight: 500; color: palette(window-text);");
    bannerLayout->addWidget(m_summaryLabel);

    m_compatibilityLabel->setWordWrap(true);
    bannerLayout->addWidget(m_compatibilityLabel);

    rightLayout->addWidget(m_bannerContainer);

    // Options
    rightLayout->addWidget(m_forceReencodeCheckBox);

    // Output Destination
    auto* outputLabel = new QLabel("Output Destination", this);
    outputLabel->setStyleSheet("font-weight: bold; color: palette(window-text);");
    auto* outputLayout = new QHBoxLayout();
    m_outputPath->setPlaceholderText("Select destination MP4 file...");
    outputLayout->addWidget(m_outputPath, 1);
    outputLayout->addWidget(m_outputButton);
    rightLayout->addWidget(outputLabel);
    rightLayout->addLayout(outputLayout);

    rightLayout->addStretch(1);

    // Actions & Progress (matching Tab 1 & Tab 2 exactly!)
    auto* actionLayout = new QHBoxLayout();
    m_combineButton->setStyleSheet("padding: 6px 14px; font-weight: bold;");
    actionLayout->addWidget(m_combineButton);
    actionLayout->addWidget(m_cancelButton);
    m_cancelButton->setEnabled(false);

    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);

    rightLayout->addLayout(actionLayout);
    rightLayout->addWidget(m_progressBar);
    rightLayout->addWidget(m_statusLabel);

    // Assemble root two-column layout (equal 1:1 split matching Tab 1 & Tab 2)
    leftWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rightWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rootLayout->addWidget(leftWidget, 1);
    rootLayout->addWidget(rightWidget, 1);

    // ==================== CONNECTIONS ====================
    connect(m_placeholderAddButton, &QPushButton::clicked, this, &VideoCombineWidget::onAddVideosClicked);
    connect(m_addVideosButton, &QPushButton::clicked, this, &VideoCombineWidget::onAddVideosClicked);
    connect(m_moveUpButton, &QPushButton::clicked, this, &VideoCombineWidget::onMoveUpClicked);
    connect(m_moveDownButton, &QPushButton::clicked, this, &VideoCombineWidget::onMoveDownClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &VideoCombineWidget::onRemoveSelectedClicked);
    connect(m_clearAllButton, &QPushButton::clicked, this, &VideoCombineWidget::onClearAllClicked);
    connect(m_clipListWidget, &QListWidget::currentRowChanged, this, &VideoCombineWidget::onQueueSelectionChanged);

    connect(m_outputButton, &QPushButton::clicked, this, &VideoCombineWidget::onBrowseOutputClicked);
    connect(m_outputPath, &QLineEdit::textEdited, this, [this]() {
        m_userCustomizedOutput = true;
        updateControlsState();
    });

    connect(m_forceReencodeCheckBox, &QCheckBox::toggled, this, &VideoCombineWidget::onForceReencodeToggled);
    connect(m_combineButton, &QPushButton::clicked, this, &VideoCombineWidget::onCombineClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &VideoCombineWidget::onCancelClicked);

    // MediaService connections
    connect(&m_mediaService, &MediaService::videoCombineStarted, this, &VideoCombineWidget::onVideoCombineStarted);
    connect(&m_mediaService, &MediaService::progressChanged, this, &VideoCombineWidget::onProgressChanged);
    connect(&m_mediaService, &MediaService::videoCombineCancelled, this, &VideoCombineWidget::onVideoCombineCancelled);
    connect(&m_mediaService, &MediaService::videoCombineFinished, this, &VideoCombineWidget::onVideoCombineFinished);

    updateCompatibilityBanner();
    updateControlsState();
}

void VideoCombineWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void VideoCombineWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void VideoCombineWidget::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    QStringList validFiles;

    const QStringList extensions = {"mp4", "mkv", "mov", "webm", "avi"};

    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            const QString localPath = url.toLocalFile();
            const QString suffix = QFileInfo(localPath).suffix().toLower();
            if (extensions.contains(suffix)) {
                validFiles.append(localPath);
            }
        }
    }

    if (!validFiles.isEmpty()) {
        addVideoFiles(validFiles);
        event->acceptProposedAction();
    }
}

void VideoCombineWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

void VideoCombineWidget::onAddVideosClicked()
{
    const QString filter = "Video Files (*.mp4 *.mkv *.mov *.webm *.avi);;All Files (*.*)";
    const QStringList paths = QFileDialog::getOpenFileNames(
        this,
        "Select Video Files to Combine",
        QString(),
        filter
    );

    if (!paths.isEmpty()) {
        addVideoFiles(paths);
    }
}

void VideoCombineWidget::addVideoFiles(const QStringList& filePaths)
{
    for (const QString& path : filePaths) {
        VideoClipEntry entry;
        entry.filePath = path;
        entry.probed = false;

        // Create placeholder thumbnail (adaptive card)
        QPixmap placeholder(64, 40);
        const bool isDark = palette().color(QPalette::Window).lightness() < 128;
        placeholder.fill(isDark ? QColor("#334155") : QColor("#e2e8f0"));
        QPainter p(&placeholder);
        p.setPen(isDark ? QColor("#94a3b8") : QColor("#64748b"));
        p.drawText(placeholder.rect(), Qt::AlignCenter, "Video");
        entry.thumbnail = placeholder;

        m_clips.append(entry);

        auto* item = new QListWidgetItem(m_clipListWidget);
        item->setIcon(QIcon(entry.thumbnail));
        item->setText(QString("%1. %2\nProbing metadata...")
                          .arg(m_clips.size())
                          .arg(QFileInfo(path).fileName()));
    }

    if (m_clipListWidget->currentRow() < 0 && !m_clips.isEmpty()) {
        m_clipListWidget->setCurrentRow(0);
    }

    suggestDefaultOutputPath();
    updateControlsState();
    probeNextUnprobedClip();
}

void VideoCombineWidget::probeNextUnprobedClip()
{
    int unprobedIndex = -1;
    for (int i = 0; i < m_clips.size(); ++i) {
        if (!m_clips[i].probed) {
            unprobedIndex = i;
            break;
        }
    }

    if (unprobedIndex == -1) {
        // All probed
        updateCompatibilityBanner();
        updateControlsState();
        return;
    }

    // Schedule probe asynchronously to keep UI buttery smooth
    QTimer::singleShot(10, this, [this, unprobedIndex]() {
        if (unprobedIndex >= m_clips.size()) {
            return;
        }

        VideoClipEntry& entry = m_clips[unprobedIndex];
        m_mediaService.getVideoMetadata(entry.filePath, entry.metadata);

        // Extract thumbnail at 10% of duration
        const double thumbTs = std::max(0.1, entry.metadata.durationSeconds * 0.1);
        QPixmap thumb;
        if (m_mediaService.extractThumbnail(entry.filePath, thumbTs, thumb)) {
            entry.thumbnail = thumb.scaled(320, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        entry.probed = true;
        updateClipItemWidget(unprobedIndex);
        probeNextUnprobedClip();
    });
}

void VideoCombineWidget::updateClipItemWidget(int index)
{
    if (index < 0 || index >= m_clips.size() || index >= m_clipListWidget->count()) {
        return;
    }

    const VideoClipEntry& entry = m_clips[index];
    auto* item = m_clipListWidget->item(index);

    if (!entry.thumbnail.isNull()) {
        item->setIcon(QIcon(entry.thumbnail.scaled(56, 35, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)));
    }

    const QString fileName = QFileInfo(entry.filePath).fileName();
    if (entry.probed) {
        const QString vCodec = entry.metadata.videoCodec.isEmpty() ? "Unknown" : entry.metadata.videoCodec;
        const QString aCodec = entry.metadata.audioCodec.isEmpty() ? "No Audio" : entry.metadata.audioCodec;

        item->setText(
            QString("%1. %2\n%3x%4 • %5 • %6 / %7 • %8")
                .arg(index + 1)
                .arg(fileName)
                .arg(entry.metadata.width)
                .arg(entry.metadata.height)
                .arg(formatDuration(entry.metadata.durationSeconds))
                .arg(vCodec)
                .arg(aCodec)
                .arg(formatFileSize(entry.metadata.fileSizeBytes))
        );
    } else {
        item->setText(QString("%1. %2\nProbing metadata...").arg(index + 1).arg(fileName));
    }
}

void VideoCombineWidget::updateCompatibilityBanner()
{
    const bool isDark = palette().color(QPalette::Window).lightness() < 128;

    if (m_clips.isEmpty()) {
        m_summaryLabel->setText("No clips in queue.");
        m_compatibilityLabel->setText("Add video files using the '+ Add Videos...' button or drag and drop files here.");
        m_compatibilityLabel->setStyleSheet(isDark ? "color: #94a3b8; font-size: 11px;" : "color: #64748b; font-size: 11px;");
        return;
    }

    double totalDuration = 0.0;
    qint64 totalBytes = 0;
    int maxArea = 0;
    int maxW = 1920;
    int maxH = 1080;

    bool allMatching = true;
    bool allProbed = true;

    for (int i = 0; i < m_clips.size(); ++i) {
        const auto& clip = m_clips[i];
        if (!clip.probed) {
            allProbed = false;
        }

        totalDuration += clip.metadata.durationSeconds;
        totalBytes += clip.metadata.fileSizeBytes;

        const int area = clip.metadata.width * clip.metadata.height;
        if (area > maxArea) {
            maxArea = area;
            maxW = clip.metadata.width;
            maxH = clip.metadata.height;
        }

        if (i > 0) {
            const auto& first = m_clips[0];
            if (clip.metadata.width != first.metadata.width ||
                clip.metadata.height != first.metadata.height ||
                clip.metadata.videoCodec != first.metadata.videoCodec ||
                clip.metadata.audioCodec != first.metadata.audioCodec) {
                allMatching = false;
            }
        }
    }

    maxW = (maxW + 1) & ~1;
    maxH = (maxH + 1) & ~1;

    m_summaryLabel->setText(
        QString("%1 Clip%2  •  Total Duration: %3  •  Estimated Size: %4")
            .arg(m_clips.size())
            .arg(m_clips.size() == 1 ? "" : "s")
            .arg(formatDuration(totalDuration))
            .arg(formatFileSize(totalBytes))
    );

    if (m_clips.size() < 2) {
        m_compatibilityLabel->setText("Add at least 2 videos to evaluate format compatibility.");
        m_compatibilityLabel->setStyleSheet(isDark ? "color: #94a3b8; font-size: 11px;" : "color: #64748b; font-size: 11px;");
        return;
    }

    if (!allProbed) {
        m_compatibilityLabel->setText("Probing format compatibility...");
        m_compatibilityLabel->setStyleSheet("color: #1976d2; font-size: 11px; font-weight: 500;");
        return;
    }

    if (m_forceReencodeCheckBox->isChecked()) {
        m_compatibilityLabel->setText(
            QString("⚙ Force Re-encode enabled: Clips will be unified to standard H.264/AAC on a %1x%2 canvas.")
                .arg(maxW).arg(maxH)
        );
        m_compatibilityLabel->setStyleSheet(
            isDark ? "background-color: rgba(126, 87, 194, 0.25); color: #d1c4e9; border: 1px solid #7e57c2; "
                     "border-radius: 4px; padding: 6px 8px; font-size: 11px; font-weight: 500;"
                   : "background-color: #ede7f6; color: #4527a0; border: 1px solid #d1c4e9; "
                     "border-radius: 4px; padding: 6px 8px; font-size: 11px; font-weight: 500;"
        );
    } else if (allMatching) {
        const auto& meta = m_clips[0].metadata;
        m_compatibilityLabel->setText(
            QString("✓ Matching Formats (%1x%2, %3 / %4) — Fast lossless stream copy will be used.")
                .arg(meta.width).arg(meta.height).arg(meta.videoCodec).arg(meta.audioCodec)
        );
        m_compatibilityLabel->setStyleSheet(
            isDark ? "background-color: rgba(46, 125, 50, 0.25); color: #81c784; border: 1px solid #388e3c; "
                     "border-radius: 4px; padding: 6px 8px; font-size: 11px; font-weight: 500;"
                   : "background-color: #e8f5e9; color: #2e7d32; border: 1px solid #a5d6a7; "
                     "border-radius: 4px; padding: 6px 8px; font-size: 11px; font-weight: 500;"
        );
    } else {
        m_compatibilityLabel->setText(
            QString("⚠ Mismatched formats detected. Smart re-encode will normalize clips onto a high-res %1x%2 canvas without distortion.")
                .arg(maxW).arg(maxH)
        );
        m_compatibilityLabel->setStyleSheet(
            isDark ? "background-color: rgba(245, 127, 23, 0.25); color: #ffb74d; border: 1px solid #f57f17; "
                     "border-radius: 4px; padding: 6px 8px; font-size: 11px; font-weight: 500;"
                   : "background-color: #fff8e1; color: #b78103; border: 1px solid #ffe082; "
                     "border-radius: 4px; padding: 6px 8px; font-size: 11px; font-weight: 500;"
        );
    }
}

void VideoCombineWidget::onMoveUpClicked()
{
    const int row = m_clipListWidget->currentRow();
    if (row > 0) {
        m_clips.swapItemsAt(row, row - 1);

        for (int i = 0; i < m_clips.size(); ++i) {
            updateClipItemWidget(i);
        }

        m_clipListWidget->setCurrentRow(row - 1);
        suggestDefaultOutputPath();
        updateCompatibilityBanner();
        updateControlsState();
    }
}

void VideoCombineWidget::onMoveDownClicked()
{
    const int row = m_clipListWidget->currentRow();
    if (row >= 0 && row < m_clips.size() - 1) {
        m_clips.swapItemsAt(row, row + 1);

        for (int i = 0; i < m_clips.size(); ++i) {
            updateClipItemWidget(i);
        }

        m_clipListWidget->setCurrentRow(row + 1);
        suggestDefaultOutputPath();
        updateCompatibilityBanner();
        updateControlsState();
    }
}

void VideoCombineWidget::onRemoveSelectedClicked()
{
    const int row = m_clipListWidget->currentRow();
    if (row >= 0 && row < m_clips.size()) {
        m_clips.removeAt(row);
        delete m_clipListWidget->takeItem(row);

        for (int i = 0; i < m_clips.size(); ++i) {
            updateClipItemWidget(i);
        }

        suggestDefaultOutputPath();
        updateCompatibilityBanner();
        updateControlsState();
    }
}

void VideoCombineWidget::onClearAllClicked()
{
    if (m_clips.isEmpty()) {
        return;
    }

    m_clips.clear();
    m_clipListWidget->clear();

    if (!m_userCustomizedOutput) {
        m_outputPath->clear();
    }

    updateCompatibilityBanner();
    updateControlsState();
}

void VideoCombineWidget::onQueueSelectionChanged()
{
    const int row = m_clipListWidget->currentRow();
    const bool hasSelection = (row >= 0 && row < m_clips.size());

    m_moveUpButton->setEnabled(hasSelection && row > 0 && !m_mediaService.isProcessing());
    m_moveDownButton->setEnabled(hasSelection && row < m_clips.size() - 1 && !m_mediaService.isProcessing());
    m_removeButton->setEnabled(hasSelection && !m_mediaService.isProcessing());
}

void VideoCombineWidget::onForceReencodeToggled(bool)
{
    updateCompatibilityBanner();
}

void VideoCombineWidget::onBrowseOutputClicked()
{
    const QString defaultPath = m_outputPath->text().trimmed();
    const QString selected = QFileDialog::getSaveFileName(
        this,
        "Choose Combined Video Destination",
        defaultPath.isEmpty() ? QString() : defaultPath,
        "MP4 Video (*.mp4);;All Files (*.*)"
    );

    if (!selected.isEmpty()) {
        m_outputPath->setText(selected);
        m_userCustomizedOutput = true;
        updateControlsState();
    }
}

void VideoCombineWidget::suggestDefaultOutputPath()
{
    if (m_userCustomizedOutput || m_clips.isEmpty()) {
        return;
    }

    const QFileInfo firstClip(m_clips[0].filePath);
    const QString suggested = firstClip.absoluteDir().filePath(
        QString("%1_combined.mp4").arg(firstClip.completeBaseName())
    );
    m_outputPath->setText(suggested);
}

void VideoCombineWidget::updateControlsState()
{
    const bool busy = m_mediaService.isProcessing();
    const bool hasClips = !m_clips.isEmpty();
    const bool canCombine = !busy && m_clips.size() >= 2 && !m_outputPath->text().trimmed().isEmpty();

    m_placeholderCard->setVisible(!hasClips);
    m_clipListWidget->setVisible(hasClips);

    m_combineButton->setEnabled(canCombine);
    m_cancelButton->setEnabled(busy);
    m_placeholderAddButton->setEnabled(!busy);
    m_addVideosButton->setEnabled(!busy);
    m_clearAllButton->setEnabled(!busy && hasClips);
    m_forceReencodeCheckBox->setEnabled(!busy);
    m_outputButton->setEnabled(!busy);
    m_outputPath->setEnabled(!busy);

    if (hasClips && m_clipListWidget->currentRow() < 0) {
        m_clipListWidget->setCurrentRow(0);
    }

    onQueueSelectionChanged();
}

void VideoCombineWidget::setUiBusy(bool busy)
{
    m_combineButton->setEnabled(!busy);
    m_cancelButton->setEnabled(busy);
    m_placeholderAddButton->setEnabled(!busy);
    m_addVideosButton->setEnabled(!busy);
    m_moveUpButton->setEnabled(!busy);
    m_moveDownButton->setEnabled(!busy);
    m_removeButton->setEnabled(!busy);
    m_clearAllButton->setEnabled(!busy);
    m_forceReencodeCheckBox->setEnabled(!busy);
    m_outputButton->setEnabled(!busy);
    m_outputPath->setEnabled(!busy);
}

void VideoCombineWidget::onCombineClicked()
{
    if (m_clips.size() < 2) {
        QMessageBox::warning(this, "Picture This", "Please add at least two videos to combine.");
        return;
    }

    const QString output = m_outputPath->text().trimmed();
    if (output.isEmpty()) {
        QMessageBox::warning(this, "Picture This", "Please select an output file destination.");
        return;
    }

    // Verify output isn't one of the inputs
    for (const auto& clip : m_clips) {
        if (QFileInfo(clip.filePath) == QFileInfo(output)) {
            QMessageBox::warning(this, "Picture This", "Output destination cannot overwrite one of the input videos.");
            return;
        }
    }

    QStringList inputPaths;
    inputPaths.reserve(m_clips.size());
    for (const auto& clip : m_clips) {
        inputPaths.append(clip.filePath);
    }

    QString errorMsg;
    const bool ok = m_mediaService.combineVideos(
        inputPaths,
        output,
        m_forceReencodeCheckBox->isChecked(),
        &errorMsg
    );

    if (!ok) {
        QMessageBox::critical(this, "Error Starting Combination", errorMsg);
    }
}

void VideoCombineWidget::onCancelClicked()
{
    m_mediaService.cancel();
    m_statusLabel->setText("Cancelling video combination...");
}

void VideoCombineWidget::onVideoCombineStarted()
{
    setUiBusy(true);
    m_progressBar->setValue(0);
    m_statusLabel->setText("Combining video clips...");
}

void VideoCombineWidget::onProgressChanged(int percentage)
{
    m_progressBar->setValue(percentage);
    m_statusLabel->setText(QString("Combining videos: %1%").arg(percentage));
}

void VideoCombineWidget::onVideoCombineCancelled()
{
    setUiBusy(false);
    updateControlsState();
    m_progressBar->setValue(0);
    m_statusLabel->setText("Video combination cancelled.");
}

void VideoCombineWidget::onVideoCombineFinished(bool success, const QString& errorMessage)
{
    setUiBusy(false);
    updateControlsState();

    if (success) {
        m_progressBar->setValue(100);
        m_statusLabel->setText("Video combined successfully!");
        QMessageBox::information(this, "Picture This", "Videos have been combined successfully!");
    } else {
        m_statusLabel->setText("Video combination failed.");
        QMessageBox::critical(this, "Combination Failed", errorMessage.isEmpty() ? "Unknown error occurred." : errorMessage);
    }
}

QString VideoCombineWidget::formatDuration(double seconds)
{
    if (seconds <= 0.0) {
        return "00:00";
    }

    const int totalSec = static_cast<int>(seconds + 0.5);
    const int hours = totalSec / 3600;
    const int minutes = (totalSec % 3600) / 60;
    const int secs = totalSec % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }

    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}

QString VideoCombineWidget::formatFileSize(qint64 bytes)
{
    if (bytes <= 0) {
        return "0 MB";
    }

    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (mb >= 1024.0) {
        return QString("%1 GB").arg(mb / 1024.0, 0, 'f', 2);
    }
    return QString("%1 MB").arg(mb, 0, 'f', 1);
}
