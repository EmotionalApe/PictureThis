#pragma once

#include <QWidget>
#include <QString>
#include <QPixmap>
#include "../media/MediaService.h"
#include "../media/InspectionTypes.h"

class InspectionScheduler;
class QLabel;
class QPushButton;
class QTableWidget;
class QFrame;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QResizeEvent;

class VideoCompareWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoCompareWidget(
        MediaService& mediaService,
        InspectionScheduler& scheduler,
        QWidget* parent = nullptr
    );
    ~VideoCompareWidget() override = default;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void chooseVideoA();
    void chooseVideoB();
    void clearVideoA();
    void clearVideoB();
    void clearAll();
    void swapVideos();

    void onInspectionMetadataReady(const ProbeResult& result);
    void onInspectionThumbnailReady(const ThumbnailResult& result);

private:
    void loadVideoA(const QString& path);
    void loadVideoB(const QString& path);
    void updateTable();
    void updateDiagnostics();
    void updateThumbnailCardA();
    void updateThumbnailCardB();

    QWidget* createVideoCard(
        const QString& headerTitle,
        QLabel*& previewLabel,
        QLabel*& nameLabel,
        QLabel*& badgeLabel,
        QPushButton*& selectBtn,
        QPushButton*& clearBtn,
        bool isCardA
    );

    MediaService& m_mediaService;
    InspectionScheduler& m_scheduler;

    // State for Video A
    QString m_filePathA;
    VideoMetadata m_metaA;
    bool m_hasMetaA = false;
    QPixmap m_pixmapA;
    RequestId m_pendingMetaIdA;
    RequestId m_pendingThumbIdA;

    // State for Video B
    QString m_filePathB;
    VideoMetadata m_metaB;
    bool m_hasMetaB = false;
    QPixmap m_pixmapB;
    RequestId m_pendingMetaIdB;
    RequestId m_pendingThumbIdB;

    // UI elements
    QLabel* m_previewLabelA = nullptr;
    QLabel* m_nameLabelA = nullptr;
    QLabel* m_badgeLabelA = nullptr;
    QPushButton* m_selectBtnA = nullptr;
    QPushButton* m_clearBtnA = nullptr;

    QLabel* m_previewLabelB = nullptr;
    QLabel* m_nameLabelB = nullptr;
    QLabel* m_badgeLabelB = nullptr;
    QPushButton* m_selectBtnB = nullptr;
    QPushButton* m_clearBtnB = nullptr;

    QPushButton* m_swapBtn = nullptr;
    QPushButton* m_clearAllBtn = nullptr;

    QTableWidget* m_tableWidget = nullptr;

    QFrame* m_diagnosticsCard = nullptr;
    QLabel* m_diagnosticsTitle = nullptr;
    QLabel* m_diagnosticsContent = nullptr;
};
