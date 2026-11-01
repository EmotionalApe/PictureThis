#pragma once

#include <QMainWindow>

class MediaService;
class ImageAudioWidget;
class VideoTrimWidget;
class VideoCombineWidget;
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
    QTabWidget* m_tabWidget;
    ImageAudioWidget* m_imageAudioWidget;
    VideoTrimWidget* m_videoTrimWidget;
    VideoCombineWidget* m_videoCombineWidget;

    QWidget* m_alertBanner = nullptr;
    QLabel* m_alertLabel = nullptr;
    QPushButton* m_settingsBtn = nullptr;
};