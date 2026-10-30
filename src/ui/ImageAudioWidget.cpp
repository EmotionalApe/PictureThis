#include "ImageAudioWidget.h"

#include "../media/MediaService.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

#include <numeric>

ImageAudioWidget::ImageAudioWidget(
    MediaService& mediaService,
    QWidget* parent)
    : QWidget(parent),
      m_mediaService(mediaService),
      m_previewLabel(new QLabel(this)),
      m_imageInfoLabel(new QLabel("No image loaded", this)),
      m_imagePath(new QLineEdit(this)),
      m_audioPath(new QLineEdit(this)),
      m_outputPath(new QLineEdit(this)),
      m_imageButton(new QPushButton("Browse...", this)),
      m_pasteButton(new QPushButton("Paste", this)),
      m_audioButton(new QPushButton("Browse...", this)),
      m_audioInfoLabel(new QLabel(this)),
      m_outputButton(new QPushButton("Browse...", this)),
      m_createButton(new QPushButton("Create Video", this)),
      m_cancelButton(new QPushButton("Cancel", this)),
      m_progressBar(new QProgressBar(this)),
      m_statusLabel(new QLabel("Ready", this))
{
    setAcceptDrops(true);

    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(20);

    // --- Left column: Image preview & drop area ---
    auto* leftWidget = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    auto* previewHeading = new QLabel("Image Preview", this);
    previewHeading->setStyleSheet("font-weight: bold; font-size: 13px; color: palette(window-text);");
    leftLayout->addWidget(previewHeading);

    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(260, 260);
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setText("No image selected\n\nDrag & drop an image or audio here\nor click Paste");
    m_previewLabel->setStyleSheet(
        "border: 2px dashed #888888; border-radius: 8px; background-color: rgba(128, 128, 128, 0.08); padding: 12px; color: #555555;"
    );
    leftLayout->addWidget(m_previewLabel, 1);

    m_imageInfoLabel->setAlignment(Qt::AlignCenter);
    m_imageInfoLabel->setStyleSheet("color: palette(window-text); font-size: 11px;");
    m_imageInfoLabel->setWordWrap(true);
    leftLayout->addWidget(m_imageInfoLabel);

    // --- Right column: Inputs, Actions, Progress ---
    auto* rightWidget = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(12);

    // Image Input Row
    auto* imageLabel = new QLabel("Image", this);
    imageLabel->setStyleSheet("font-weight: bold; color: palette(window-text);");
    auto* imageLayout = new QHBoxLayout();
    imageLayout->addWidget(m_imagePath);
    imageLayout->addWidget(m_imageButton);
    imageLayout->addWidget(m_pasteButton);
    m_pasteButton->setToolTip("Paste image from clipboard");

    // Audio Input Row
    auto* audioLabel = new QLabel("Audio", this);
    audioLabel->setStyleSheet("font-weight: bold; color: palette(window-text);");
    auto* audioLayout = new QHBoxLayout();
    audioLayout->addWidget(m_audioPath);
    audioLayout->addWidget(m_audioButton);

    m_audioInfoLabel->setStyleSheet("color: palette(window-text); font-size: 11px;");

    // Output Input Row
    auto* outputLabel = new QLabel("Output", this);
    outputLabel->setStyleSheet("font-weight: bold; color: palette(window-text);");
    auto* outputLayout = new QHBoxLayout();
    outputLayout->addWidget(m_outputPath);
    outputLayout->addWidget(m_outputButton);

    rightLayout->addWidget(imageLabel);
    rightLayout->addLayout(imageLayout);

    rightLayout->addWidget(audioLabel);
    rightLayout->addLayout(audioLayout);
    rightLayout->addWidget(m_audioInfoLabel);

    rightLayout->addWidget(outputLabel);
    rightLayout->addLayout(outputLayout);

    rightLayout->addStretch(1);

    // Actions & Progress
    auto* actionLayout = new QHBoxLayout();
    m_createButton->setStyleSheet("padding: 6px 14px; font-weight: bold;");
    actionLayout->addWidget(m_createButton);
    actionLayout->addWidget(m_cancelButton);
    m_cancelButton->setEnabled(false);

    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);

    rightLayout->addLayout(actionLayout);
    rightLayout->addWidget(m_progressBar);
    rightLayout->addWidget(m_statusLabel);

    // Assemble root layout (balanced 1:1 split matching all tabs)
    rootLayout->addWidget(leftWidget, 1);
    rootLayout->addWidget(rightWidget, 1);

    // Wire up events and connections
    connect(m_imageButton, &QPushButton::clicked,
            this, &ImageAudioWidget::chooseImage);

    connect(m_pasteButton, &QPushButton::clicked,
            this, &ImageAudioWidget::pasteImageFromClipboard);

    connect(m_audioButton, &QPushButton::clicked,
            this, &ImageAudioWidget::chooseAudio);

    connect(m_outputButton, &QPushButton::clicked,
            this, &ImageAudioWidget::chooseOutput);

    connect(m_createButton, &QPushButton::clicked,
            this, &ImageAudioWidget::createVideo);

    connect(m_cancelButton, &QPushButton::clicked,
            this, &ImageAudioWidget::cancelVideo);

    connect(m_imagePath, &QLineEdit::textChanged,
            this, &ImageAudioWidget::onImagePathChanged);

    connect(m_audioPath, &QLineEdit::textChanged,
            this, &ImageAudioWidget::onAudioPathChanged);

    connect(&m_mediaService, &MediaService::videoCreationStarted,
            this, &ImageAudioWidget::onVideoCreationStarted);

    connect(&m_mediaService, &MediaService::progressChanged,
            this, &ImageAudioWidget::onProgressChanged);

    connect(&m_mediaService, &MediaService::videoCreationCancelled,
            this, &ImageAudioWidget::onVideoCreationCancelled);

    connect(&m_mediaService, &MediaService::videoCreationFinished,
            this, &ImageAudioWidget::onVideoCreationFinished);
}

ImageAudioWidget::~ImageAudioWidget()
{
    cleanupPastedTempFile();
}

void ImageAudioWidget::chooseImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Choose Image",
        QString(),
        "Images (*.png *.jpg *.jpeg *.webp)"
    );

    if (!path.isEmpty()) {
        setImagePath(path);
    }
}

void ImageAudioWidget::chooseAudio()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Choose Audio",
        QString(),
        "Audio (*.mp3 *.m4a)"
    );

    if (!path.isEmpty()) {
        setAudioPath(path);
    }
}

void ImageAudioWidget::chooseOutput()
{
    const QString path = QFileDialog::getSaveFileName(
        this,
        "Choose Output",
        "output.mp4",
        "MP4 Video (*.mp4)"
    );

    if (!path.isEmpty()) {
        m_outputPath->setText(path);
    }
}

void ImageAudioWidget::setImagePath(const QString& path)
{
    const QString trimmed = path.trimmed();
    if (trimmed != m_pastedTempFilePath) {
        cleanupPastedTempFile();
    }
    m_imagePath->setText(trimmed);
}

void ImageAudioWidget::setAudioPath(const QString& path)
{
    m_audioPath->setText(path.trimmed());
}

void ImageAudioWidget::onImagePathChanged(const QString& path)
{
    const QString trimmed = path.trimmed();
    if (trimmed != m_pastedTempFilePath) {
        cleanupPastedTempFile();
    }
    updateImagePreview(trimmed);
}

void ImageAudioWidget::onAudioPathChanged(const QString& path)
{
    updateAudioDetails(path.trimmed());
}

void ImageAudioWidget::updateImagePreview(const QString& path)
{
    if (path.isEmpty() || !QFile::exists(path)) {
        m_loadedPixmap = QPixmap();
        renderPreviewThumbnail();
        m_imageInfoLabel->setText("No image loaded");
        return;
    }

    const QImage img(path);
    if (img.isNull()) {
        m_loadedPixmap = QPixmap();
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText("Could not load image\n(unsupported or corrupted format)");
        m_previewLabel->setStyleSheet(
            "border: 2px dashed #cc4444; border-radius: 8px; background-color: rgba(200, 50, 50, 0.06); padding: 12px; color: #aa2222;"
        );
        m_imageInfoLabel->setText("Unsupported image format");
        return;
    }

    m_loadedPixmap = QPixmap::fromImage(img);
    renderPreviewThumbnail();

    const int w = img.width();
    const int h = img.height();

    QString ratioStr;
    const int g = std::gcd(w, h);
    if (g > 0 && (w / g) <= 32 && (h / g) <= 32) {
        ratioStr = QString("%1:%2").arg(w / g).arg(h / g);
    } else if (h > 0) {
        const double aspect = static_cast<double>(w) / static_cast<double>(h);
        ratioStr = QString("%1:1").arg(aspect, 0, 'f', 2);
    }

    const qint64 bytes = QFileInfo(path).size();
    QString sizeStr;
    if (bytes < 1024 * 1024) {
        sizeStr = QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    } else {
        sizeStr = QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    }

    m_imageInfoLabel->setText(QString("%1 × %2 (%3) — %4").arg(w).arg(h).arg(ratioStr, sizeStr));
}

void ImageAudioWidget::renderPreviewThumbnail()
{
    if (m_loadedPixmap.isNull()) {
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText("No image selected\n\nDrag & drop an image or audio here\nor click Paste");
        m_previewLabel->setStyleSheet(
            "border: 2px dashed #888888; border-radius: 8px; background-color: rgba(128, 128, 128, 0.08); padding: 12px; color: #555555;"
        );
        return;
    }

    const QSize targetSize = m_previewLabel->size() - QSize(16, 16);
    if (targetSize.width() <= 0 || targetSize.height() <= 0) {
        return;
    }

    const QPixmap scaled = m_loadedPixmap.scaled(
        targetSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );
    m_previewLabel->setPixmap(scaled);
    m_previewLabel->setStyleSheet(
        "border: 1px solid #aaaaaa; border-radius: 8px; background-color: rgba(0, 0, 0, 0.03); padding: 4px;"
    );
}

void ImageAudioWidget::updateAudioDetails(const QString& path)
{
    if (path.isEmpty() || !QFile::exists(path)) {
        m_audioInfoLabel->setText("");
        return;
    }

    double duration = 0.0;
    QString errorMessage;
    if (m_mediaService.getMediaDuration(path, duration, &errorMessage)) {
        const int totalSec = static_cast<int>(duration + 0.5);
        const int hrs = totalSec / 3600;
        const int mins = (totalSec % 3600) / 60;
        const int secs = totalSec % 60;

        QString durStr;
        if (hrs > 0) {
            durStr = QString::asprintf("%02d:%02d:%02d", hrs, mins, secs);
        } else {
            durStr = QString::asprintf("%02d:%02d", mins, secs);
        }

        const qint64 bytes = QFileInfo(path).size();
        QString sizeStr;
        if (bytes < 1024 * 1024) {
            sizeStr = QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
        } else {
            sizeStr = QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
        }

        m_audioInfoLabel->setText(QString("Duration: %1  |  Size: %2").arg(durStr, sizeStr));
    } else {
        m_audioInfoLabel->setText("Could not determine audio duration");
    }
}

void ImageAudioWidget::pasteImageFromClipboard()
{
    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard) {
        return;
    }

    const QMimeData* mimeData = clipboard->mimeData();
    if (!mimeData) {
        return;
    }

    // 1. Raw image data (e.g. screenshot or copied web image)
    if (mimeData->hasImage()) {
        const QImage img = qvariant_cast<QImage>(mimeData->imageData());
        if (!img.isNull()) {
            cleanupPastedTempFile();

            const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            const QString tempFile = QDir(tempDir).filePath(
                QString("picture_this_paste_%1.png").arg(QDateTime::currentMSecsSinceEpoch())
            );

            if (img.save(tempFile, "PNG")) {
                m_pastedTempFilePath = tempFile;
                m_imagePath->setText(tempFile);
                m_statusLabel->setText("Pasted image from clipboard.");
                return;
            }
        }
    }

    // 2. Copied file URL from file manager
    if (mimeData->hasUrls()) {
        const QList<QUrl> urls = mimeData->urls();
        for (const QUrl& url : urls) {
            const QString localFile = url.toLocalFile();
            const QString suffix = QFileInfo(localFile).suffix().toLower();
            if (suffix == "png" || suffix == "jpg" || suffix == "jpeg" || suffix == "webp") {
                if (QFile::exists(localFile)) {
                    setImagePath(localFile);
                    m_statusLabel->setText("Pasted image file from clipboard.");
                    return;
                }
            }
        }
    }

    QMessageBox::information(
        this,
        "Paste Image",
        "No image or image file found on the clipboard."
    );
}

void ImageAudioWidget::cleanupPastedTempFile()
{
    if (!m_pastedTempFilePath.isEmpty()) {
        if (QFile::exists(m_pastedTempFilePath)) {
            QFile::remove(m_pastedTempFilePath);
        }
        m_pastedTempFilePath.clear();
    }
}

void ImageAudioWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (m_mediaService.isProcessing()) {
        event->ignore();
        return;
    }

    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl& url : urls) {
            const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
            if (suffix == "png" || suffix == "jpg" || suffix == "jpeg" || suffix == "webp"
                || suffix == "mp3" || suffix == "m4a") {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void ImageAudioWidget::dragMoveEvent(QDragMoveEvent* event)
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

void ImageAudioWidget::dropEvent(QDropEvent* event)
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
            if (suffix == "png" || suffix == "jpg" || suffix == "jpeg" || suffix == "webp") {
                setImagePath(localPath);
            } else if (suffix == "mp3" || suffix == "m4a") {
                setAudioPath(localPath);
            }
        }
        event->acceptProposedAction();
    }
}

void ImageAudioWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!m_loadedPixmap.isNull()) {
        renderPreviewThumbnail();
    }
}

void ImageAudioWidget::setUiBusy(bool busy)
{
    m_createButton->setEnabled(!busy);
    m_cancelButton->setEnabled(busy);
    m_imageButton->setEnabled(!busy);
    m_pasteButton->setEnabled(!busy);
    m_audioButton->setEnabled(!busy);
    m_outputButton->setEnabled(!busy);

    m_imagePath->setEnabled(!busy);
    m_audioPath->setEnabled(!busy);
    m_outputPath->setEnabled(!busy);

    if (busy) {
        m_progressBar->setValue(0);
    }
}

void ImageAudioWidget::createVideo()
{
    const QString imagePath = m_imagePath->text().trimmed();
    const QString audioPath = m_audioPath->text().trimmed();
    const QString outputPath = m_outputPath->text().trimmed();

    if (imagePath.isEmpty()) {
        QMessageBox::warning(
            this,
            "Missing Image",
            "Please choose an image."
        );
        return;
    }

    if (audioPath.isEmpty()) {
        QMessageBox::warning(
            this,
            "Missing Audio",
            "Please choose an audio file."
        );
        return;
    }

    if (outputPath.isEmpty()) {
        QMessageBox::warning(
            this,
            "Missing Output",
            "Please choose an output location."
        );
        return;
    }

    if (!QFile::exists(imagePath)) {
        QMessageBox::warning(
            this,
            "Image Not Found",
            "The selected image file does not exist on disk."
        );
        return;
    }

    if (!QFile::exists(audioPath)) {
        QMessageBox::warning(
            this,
            "Audio Not Found",
            "The selected audio file does not exist on disk."
        );
        return;
    }

    const QFileInfo outInfo(outputPath);
    if (!outInfo.dir().exists()) {
        QMessageBox::warning(
            this,
            "Invalid Output Location",
            "The destination directory does not exist."
        );
        return;
    }

    // Safety: prevent overwriting the input files if output has the same canonical path
    const QString canImage = QFileInfo(imagePath).canonicalFilePath();
    const QString canAudio = QFileInfo(audioPath).canonicalFilePath();
    const QString canOutput = outInfo.canonicalFilePath();

    if (!canOutput.isEmpty() && (canOutput == canImage || canOutput == canAudio)) {
        QMessageBox::critical(
            this,
            "Invalid Output Path",
            "The output file cannot be the same as the input image or audio file."
        );
        return;
    }

    QString errorMessage;
    const bool started = m_mediaService.createVideoFromImageAndAudio(
        imagePath,
        audioPath,
        outputPath,
        &errorMessage
    );

    if (!started) {
        m_statusLabel->setText("Failed to start video creation.");
        QMessageBox::critical(
            this,
            "Video Creation Failed",
            errorMessage
        );
    }
}

void ImageAudioWidget::cancelVideo()
{
    m_statusLabel->setText("Cancelling video creation...");
    m_cancelButton->setEnabled(false);
    m_mediaService.cancel();
}

void ImageAudioWidget::onVideoCreationStarted()
{
    setUiBusy(true);
    m_statusLabel->setText("Creating video...");
}

void ImageAudioWidget::onProgressChanged(int percentage)
{
    m_progressBar->setValue(percentage);
}

void ImageAudioWidget::onVideoCreationCancelled()
{
    setUiBusy(false);
    m_progressBar->setValue(0);
    m_statusLabel->setText("Video creation cancelled.");

    QMessageBox::information(
        this,
        "Picture This",
        "Video creation was cancelled."
    );
}

void ImageAudioWidget::onVideoCreationFinished(bool success, const QString& errorMessage)
{
    setUiBusy(false);

    if (success) {
        m_progressBar->setValue(100);
        m_statusLabel->setText("Video created successfully.");
        QMessageBox::information(
            this,
            "Picture This",
            "The video was created successfully."
        );
    } else {
        m_progressBar->setValue(0);
        m_statusLabel->setText("Failed to create video.");
        QMessageBox::critical(
            this,
            "Video Creation Failed",
            errorMessage
        );
    }
}
