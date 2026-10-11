#include "MainWindow.h"

#include "../media/MediaService.h"

#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(
    MediaService& mediaService,
    QWidget* parent)
    : QMainWindow(parent),
      m_mediaService(mediaService),
      m_imagePath(new QLineEdit(this)),
      m_audioPath(new QLineEdit(this)),
      m_outputPath(new QLineEdit(this)),
      m_imageButton(new QPushButton("Choose Image", this)),
      m_audioButton(new QPushButton("Choose Audio", this)),
      m_outputButton(new QPushButton("Choose Output", this)),
      m_createButton(new QPushButton("Create Video", this)),
      m_cancelButton(new QPushButton("Cancel", this)),
      m_progressBar(new QProgressBar(this)),
      m_statusLabel(new QLabel("Ready", this))
{
    setWindowTitle("Picture This");
    resize(600, 420);

    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(centralWidget);

    auto* imageLabel = new QLabel("Image", this);
    auto* imageLayout = new QHBoxLayout();
    imageLayout->addWidget(m_imagePath);
    imageLayout->addWidget(m_imageButton);

    auto* audioLabel = new QLabel("Audio", this);
    auto* audioLayout = new QHBoxLayout();
    audioLayout->addWidget(m_audioPath);
    audioLayout->addWidget(m_audioButton);

    auto* outputLabel = new QLabel("Output", this);
    auto* outputLayout = new QHBoxLayout();
    outputLayout->addWidget(m_outputPath);
    outputLayout->addWidget(m_outputButton);

    mainLayout->addWidget(imageLabel);
    mainLayout->addLayout(imageLayout);

    mainLayout->addWidget(audioLabel);
    mainLayout->addLayout(audioLayout);

    mainLayout->addWidget(outputLabel);
    mainLayout->addLayout(outputLayout);

    mainLayout->addSpacing(20);

    auto* actionLayout = new QHBoxLayout();
    actionLayout->addWidget(m_createButton);
    actionLayout->addWidget(m_cancelButton);
    m_cancelButton->setEnabled(false);

    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);

    mainLayout->addLayout(actionLayout);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_statusLabel);

    setCentralWidget(centralWidget);

    connect(m_imageButton, &QPushButton::clicked,
            this, &MainWindow::chooseImage);

    connect(m_audioButton, &QPushButton::clicked,
            this, &MainWindow::chooseAudio);

    connect(m_outputButton, &QPushButton::clicked,
            this, &MainWindow::chooseOutput);

    connect(m_createButton, &QPushButton::clicked,
            this, &MainWindow::createVideo);

    connect(m_cancelButton, &QPushButton::clicked,
            this, &MainWindow::cancelVideo);

    connect(&m_mediaService, &MediaService::videoCreationStarted,
            this, &MainWindow::onVideoCreationStarted);

    connect(&m_mediaService, &MediaService::progressChanged,
            this, &MainWindow::onProgressChanged);

    connect(&m_mediaService, &MediaService::videoCreationCancelled,
            this, &MainWindow::onVideoCreationCancelled);

    connect(&m_mediaService, &MediaService::videoCreationFinished,
            this, &MainWindow::onVideoCreationFinished);
}

void MainWindow::chooseImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Choose Image",
        QString(),
        "Images (*.png *.jpg *.jpeg *.webp)"
    );

    if (!path.isEmpty()) {
        m_imagePath->setText(path);
    }
}

void MainWindow::chooseAudio()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Choose Audio",
        QString(),
        "Audio (*.mp3 *.m4a)"
    );

    if (!path.isEmpty()) {
        m_audioPath->setText(path);
    }
}

void MainWindow::chooseOutput()
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

void MainWindow::setUiBusy(bool busy)
{
    m_createButton->setEnabled(!busy);
    m_cancelButton->setEnabled(busy);
    m_imageButton->setEnabled(!busy);
    m_audioButton->setEnabled(!busy);
    m_outputButton->setEnabled(!busy);

    m_imagePath->setEnabled(!busy);
    m_audioPath->setEnabled(!busy);
    m_outputPath->setEnabled(!busy);

    if (busy) {
        m_progressBar->setValue(0);
    }
}

void MainWindow::createVideo()
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

void MainWindow::cancelVideo()
{
    m_statusLabel->setText("Cancelling video creation...");
    m_cancelButton->setEnabled(false);
    m_mediaService.cancel();
}

void MainWindow::onVideoCreationStarted()
{
    setUiBusy(true);
    m_statusLabel->setText("Creating video...");
}

void MainWindow::onProgressChanged(int percentage)
{
    m_progressBar->setValue(percentage);
}

void MainWindow::onVideoCreationCancelled()
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

void MainWindow::onVideoCreationFinished(bool success, const QString& errorMessage)
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

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_mediaService.isProcessing()) {
        const auto reply = QMessageBox::question(
            this,
            "Picture This",
            "A video is currently being created. Do you want to cancel and exit?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            m_mediaService.cancel();
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}