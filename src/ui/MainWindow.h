#pragma once

#include <QMainWindow>

class MediaService;
class QLineEdit;
class QLabel;
class QPushButton;
class QProgressBar;
class QCloseEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(
        MediaService& mediaService,
        QWidget* parent = nullptr
    );

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void chooseImage();
    void chooseAudio();
    void chooseOutput();
    void createVideo();
    void cancelVideo();

    void onVideoCreationStarted();
    void onProgressChanged(int percentage);
    void onVideoCreationCancelled();
    void onVideoCreationFinished(bool success, const QString& errorMessage);

private:
    void setUiBusy(bool busy);

    MediaService& m_mediaService;

    QLineEdit* m_imagePath;
    QLineEdit* m_audioPath;
    QLineEdit* m_outputPath;

    QPushButton* m_imageButton;
    QPushButton* m_audioButton;
    QPushButton* m_outputButton;
    QPushButton* m_createButton;
    QPushButton* m_cancelButton;

    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
};