#include "MainWindow.h"
#include "ImageAudioWidget.h"
#include "../media/MediaService.h"

#include <QCloseEvent>
#include <QLabel>
#include <QMessageBox>
#include <QTabWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(
    MediaService& mediaService,
    QWidget* parent)
    : QMainWindow(parent),
      m_mediaService(mediaService),
      m_tabWidget(new QTabWidget(this)),
      m_imageAudioWidget(new ImageAudioWidget(mediaService, this))
{
    setWindowTitle("Picture This");
    resize(800, 520);
    setMinimumSize(740, 460);

    m_tabWidget->setStyleSheet(
        "QTabBar::tab {"
        "    padding: 8px 18px;"
        "    font-weight: 500;"
        "    font-size: 12px;"
        "}"
        "QTabBar::tab:selected {"
        "    font-weight: bold;"
        "}"
    );

    // Tab 1: Image + Audio -> MP4
    m_tabWidget->addTab(m_imageAudioWidget, "Image to Video");

    // Tab 2: Video Trimming (Next Phase)
    m_tabWidget->addTab(
        createPlaceholderWidget(
            "Video Trimming",
            "Cut and export video segments with fast stream copy or accurate re-encoding.\n"
        ),
        "Trim Video"
    );

    // Tab 3: Video Combining (Subsequent Phase)
    m_tabWidget->addTab(
        createPlaceholderWidget(
            "Video Combining",
            "Merge multiple video clips into a single video file.\n"
        ),
        "Combine Videos"
    );

    setCentralWidget(m_tabWidget);
}

QWidget* MainWindow::createPlaceholderWidget(const QString& title, const QString& description)
{
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(title, widget);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #444444;");

    auto* descLabel = new QLabel(description, widget);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet("font-size: 13px; color: #777777; line-height: 1.4;");

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