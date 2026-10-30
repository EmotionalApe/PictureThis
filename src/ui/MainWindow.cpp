#include "MainWindow.h"
#include "ImageAudioWidget.h"
#include "VideoTrimWidget.h"
#include "VideoCombineWidget.h"
#include "../media/MediaService.h"

#include <QCloseEvent>
#include <QLabel>
#include <QMessageBox>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(
    MediaService& mediaService,
    QWidget* parent)
    : QMainWindow(parent),
      m_mediaService(mediaService),
      m_tabWidget(new QTabWidget(this)),
      m_imageAudioWidget(new ImageAudioWidget(mediaService, this)),
      m_videoTrimWidget(new VideoTrimWidget(mediaService, this)),
      m_videoCombineWidget(new VideoCombineWidget(mediaService, this))
{
    setWindowTitle("Picture This");
    resize(840, 540);
    setMinimumSize(780, 480);

    m_tabWidget->setStyleSheet(
        "QTabBar::tab {"
        "    padding: 8px 18px;"
        "    font-weight: 500;"
        "    font-size: 12px;"
        "}"
        "QTabBar::tab:selected {"
        "    font-weight: bold;"
        "}"
        "QProgressBar {"
        "    border: 1px solid palette(mid);"
        "    border-radius: 4px;"
        "    text-align: center;"
        "    background-color: rgba(128, 128, 128, 0.08);"
        "    height: 18px;"
        "    font-size: 11px;"
        "    color: palette(text);"
        "}"
        "QProgressBar::chunk {"
        "    background-color: #0d9488;"
        "    border-radius: 3px;"
        "}"
    );

    // Tab 1: Image + Audio -> MP4
    m_tabWidget->addTab(m_imageAudioWidget, "Image to Video");

    // Tab 2: Video Trimming
    m_tabWidget->addTab(m_videoTrimWidget, "Trim Video");

    // Tab 3: Video Combining
    m_tabWidget->addTab(m_videoCombineWidget, "Combine Videos");

    setCentralWidget(m_tabWidget);

    // Disable the inactive tabs while any job is running, re-enable when done.
    connect(&mediaService, &MediaService::videoCreationStarted,  this, &MainWindow::onJobStarted);
    connect(&mediaService, &MediaService::videoTrimStarted,      this, &MainWindow::onJobStarted);
    connect(&mediaService, &MediaService::videoCombineStarted,   this, &MainWindow::onJobStarted);

    connect(&mediaService, &MediaService::videoCreationFinished,  this, [this](bool, const QString&) { onJobStopped(); });
    connect(&mediaService, &MediaService::videoTrimFinished,      this, [this](bool, const QString&) { onJobStopped(); });
    connect(&mediaService, &MediaService::videoCombineFinished,   this, [this](bool, const QString&) { onJobStopped(); });
    connect(&mediaService, &MediaService::videoCreationCancelled, this, &MainWindow::onJobStopped);
    connect(&mediaService, &MediaService::videoTrimCancelled,     this, &MainWindow::onJobStopped);
    connect(&mediaService, &MediaService::videoCombineCancelled,  this, &MainWindow::onJobStopped);
}

void MainWindow::onJobStarted()
{
    const int active = m_tabWidget->currentIndex();
    QTabBar* bar = m_tabWidget->tabBar();
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        if (i != active) {
            m_tabWidget->setTabEnabled(i, false);
            bar->setTabToolTip(i, "Another operation is in progress...");
        }
    }
    // Show a forbidden cursor over the tab bar so users know the tabs are locked.
    bar->setCursor(Qt::ForbiddenCursor);
}

void MainWindow::onJobStopped()
{
    QTabBar* bar = m_tabWidget->tabBar();
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        m_tabWidget->setTabEnabled(i, true);
        bar->setTabToolTip(i, "");
    }
    bar->unsetCursor();
}

QWidget* MainWindow::createPlaceholderWidget(const QString& title, const QString& description)
{
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(title, widget);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: palette(window-text);");

    auto* descLabel = new QLabel(description, widget);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet("font-size: 13px; color: palette(placeholder-text); line-height: 1.4;");

    layout->addWidget(titleLabel);
    layout->addWidget(descLabel);

    return widget;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_mediaService.isProcessing()) {
        const auto reply = QMessageBox::question(
            this,
            "Picture This",
            "A media operation is currently in progress. Do you want to cancel and exit?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            m_mediaService.cancel();
            if (m_imageAudioWidget) {
                m_imageAudioWidget->cleanupPastedTempFile();
            }
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        if (m_imageAudioWidget) {
            m_imageAudioWidget->cleanupPastedTempFile();
        }
        event->accept();
    }
}