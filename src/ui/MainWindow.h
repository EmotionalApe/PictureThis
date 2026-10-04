#pragma once

#include <QMainWindow>

class MediaService;
class QLineEdit;
class QLabel;
class QPushButton;

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(
        MediaService& mediaService,
        QWidget* parent = nullptr
    );

private:
    void chooseImage();
    void chooseAudio();
    void chooseOutput();
    void createVideo();

    MediaService& m_mediaService;

    QLineEdit* m_imagePath;
    QLineEdit* m_audioPath;
    QLineEdit* m_outputPath;

    QLabel* m_statusLabel;

    QPushButton* m_createButton;
};