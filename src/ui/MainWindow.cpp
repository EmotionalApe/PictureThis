#include "MainWindow.h"

#include "../media/MediaService.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
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
      m_statusLabel(new QLabel("Ready", this)),
      m_createButton(new QPushButton("Create Video", this))
{
    setWindowTitle("Picture This");
    resize(600, 400);

    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(centralWidget);

    auto* imageLabel = new QLabel("Image", this);
    auto* imageButton = new QPushButton("Choose Image", this);

    auto* imageLayout = new QHBoxLayout();
    imageLayout->addWidget(m_imagePath);
    imageLayout->addWidget(imageButton);

    auto* audioLabel = new QLabel("Audio", this);
    auto* audioButton = new QPushButton("Choose Audio", this);

    auto* audioLayout = new QHBoxLayout();
    audioLayout->addWidget(m_audioPath);
    audioLayout->addWidget(audioButton);

    auto* outputLabel = new QLabel("Output", this);
    auto* outputButton = new QPushButton("Choose Output", this);

    auto* outputLayout = new QHBoxLayout();
    outputLayout->addWidget(m_outputPath);
    outputLayout->addWidget(outputButton);

    mainLayout->addWidget(imageLabel);
    mainLayout->addLayout(imageLayout);

    mainLayout->addWidget(audioLabel);
    mainLayout->addLayout(audioLayout);

    mainLayout->addWidget(outputLabel);
    mainLayout->addLayout(outputLayout);

    mainLayout->addSpacing(20);

    mainLayout->addWidget(m_createButton);
    mainLayout->addWidget(m_statusLabel);

    setCentralWidget(centralWidget);

    connect(imageButton, &QPushButton::clicked,
            this, &MainWindow::chooseImage);

    connect(audioButton, &QPushButton::clicked,
            this, &MainWindow::chooseAudio);

    connect(outputButton, &QPushButton::clicked,
            this, &MainWindow::chooseOutput);

    connect(m_createButton, &QPushButton::clicked,
            this, &MainWindow::createVideo);
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

    m_createButton->setEnabled(false);
    m_statusLabel->setText("Creating video...");

    QString errorMessage;

    const bool success =
        m_mediaService.createVideoFromImageAndAudio(
            imagePath,
            audioPath,
            outputPath,
            &errorMessage
        );

    m_createButton->setEnabled(true);

    if (success) {
        m_statusLabel->setText("Video created successfully.");

        QMessageBox::information(
            this,
            "Picture This",
            "The video was created successfully."
        );
    }
    else {
        m_statusLabel->setText("Failed to create video.");

        QMessageBox::critical(
            this,
            "Video Creation Failed",
            errorMessage
        );
    }
}