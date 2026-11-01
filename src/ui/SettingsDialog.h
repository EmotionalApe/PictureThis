#pragma once

#include <QDialog>

class MediaService;
class QLineEdit;
class QLabel;
class QPushButton;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(
        MediaService& mediaService,
        QWidget* parent = nullptr
    );
    ~SettingsDialog() override = default;

signals:
    void settingsSaved();

private slots:
    void onBrowseFfmpeg();
    void onBrowseFfprobe();
    void onTestFfmpeg();
    void onTestFfprobe();
    void onAutoDetect();
    void onSave();

private:
    void testExecutable(const QString& path, QLabel* statusLabel, const QString& expectedTool);
    void loadCurrentSettings();

    MediaService& m_mediaService;

    QLineEdit* m_ffmpegPathEdit = nullptr;
    QPushButton* m_ffmpegBrowseBtn = nullptr;
    QPushButton* m_ffmpegTestBtn = nullptr;
    QLabel* m_ffmpegStatusLabel = nullptr;

    QLineEdit* m_ffprobePathEdit = nullptr;
    QPushButton* m_ffprobeBrowseBtn = nullptr;
    QPushButton* m_ffprobeTestBtn = nullptr;
    QLabel* m_ffprobeStatusLabel = nullptr;

    QPushButton* m_autoDetectBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};
