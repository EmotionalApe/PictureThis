#pragma once

#include <QMainWindow>

class MediaService;
class InspectionScheduler;
class ImageAudioWidget;
class VideoTrimWidget;
class VideoCombineWidget;
class VideoCompressWidget;
class QTabWidget;
class QCloseEvent;
class QLabel;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(
        MediaService& mediaService,
        InspectionScheduler& scheduler,
        QWidget* parent = nullptr
    );
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent* event) override;

public slots:
    void checkToolStatus();

private slots:
    void onJobStarted();
    void onJobStopped();
    void onOpenSettings();

private:
    QWidget* createPlaceholderWidget(const QString& title, const QString& description);
    QWidget* createAlertBanner();

    MediaService& m_mediaService;
    InspectionScheduler& m_scheduler;
    QTabWidget* m_tabWidget;
    ImageAudioWidget* m_imageAudioWidget;
    VideoTrimWidget* m_videoTrimWidget;
    VideoCombineWidget* m_videoCombineWidget;
    VideoCompressWidget* m_videoCompressWidget;

    QWidget* m_alertBanner = nullptr;
    QLabel* m_alertLabel = nullptr;
    QPushButton* m_settingsBtn = nullptr;
};