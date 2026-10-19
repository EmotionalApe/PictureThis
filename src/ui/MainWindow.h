#pragma once

#include <QMainWindow>

class MediaService;
class ImageAudioWidget;
class VideoTrimWidget;
class QTabWidget;
class QCloseEvent;

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

private:
    QWidget* createPlaceholderWidget(const QString& title, const QString& description);

    MediaService& m_mediaService;
    QTabWidget* m_tabWidget;
    ImageAudioWidget* m_imageAudioWidget;
    VideoTrimWidget* m_videoTrimWidget;
};