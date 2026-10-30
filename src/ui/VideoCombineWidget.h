#pragma once

#include <QWidget>
#include <QPixmap>
#include <QList>
#include "../media/MediaService.h"

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QLabel;
class QPushButton;
class QProgressBar;
class QCheckBox;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

struct VideoClipEntry {
    QString filePath;
    VideoMetadata metadata;
    QPixmap thumbnail;
    bool probed = false;
};

class VideoCombineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoCombineWidget(
        MediaService& mediaService,
        QWidget* parent = nullptr
    );
    ~VideoCombineWidget() override = default;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onAddVideosClicked();
    void onMoveUpClicked();
    void onMoveDownClicked();
    void onRemoveSelectedClicked();
    void onClearAllClicked();
    void onQueueSelectionChanged();

    void onBrowseOutputClicked();
    void onCombineClicked();
    void onCancelClicked();
    void onForceReencodeToggled(bool checked);

    // MediaService job signals
    void onVideoCombineStarted();
    void onProgressChanged(int percentage);
    void onVideoCombineCancelled();
    void onVideoCombineFinished(bool success, const QString& errorMessage);

private:
    void addVideoFiles(const QStringList& filePaths);
    void probeNextUnprobedClip();
    void updateClipItemWidget(int index);
    void updateCompatibilityBanner();
    void updateControlsState();
    void suggestDefaultOutputPath();
    void setUiBusy(bool busy);

    static QString formatDuration(double seconds);
    static QString formatFileSize(qint64 bytes);

    MediaService& m_mediaService;
    QList<VideoClipEntry> m_clips;

    // Empty state placeholder
    QWidget* m_placeholderCard;
    QPushButton* m_placeholderAddButton;

    // Queue UI
    QListWidget* m_clipListWidget;
    QPushButton* m_addVideosButton;
    QPushButton* m_moveUpButton;
    QPushButton* m_moveDownButton;
    QPushButton* m_removeButton;
    QPushButton* m_clearAllButton;

    // Summary & Compatibility Banner
    QWidget* m_bannerContainer;
    QLabel* m_summaryLabel;
    QLabel* m_compatibilityLabel;

    // Options
    QCheckBox* m_forceReencodeCheckBox;

    // Output Destination
    QLineEdit* m_outputPath;
    QPushButton* m_outputButton;
    bool m_userCustomizedOutput = false;

    // Action Controls & Progress
    QPushButton* m_combineButton;
    QPushButton* m_cancelButton;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
};
