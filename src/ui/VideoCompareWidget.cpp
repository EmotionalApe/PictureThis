#include "VideoCompareWidget.h"
#include "../media/InspectionScheduler.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMimeData>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <cmath>

namespace {

QString formatBytes(qint64 bytes)
{
    if (bytes <= 0) return "0 B";
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

QString formatBitrate(qint64 bps)
{
    if (bps <= 0) return "N/A";
    if (bps < 1000) return QString("%1 bps").arg(bps);
    if (bps < 1000 * 1000) return QString("%1 kbps").arg(bps / 1000.0, 0, 'f', 0);
    return QString("%1 Mbps").arg(bps / 1000000.0, 0, 'f', 2);
}

QString formatDuration(double seconds)
{
    if (seconds <= 0.0) return "00:00.000";
    const int totalMs = static_cast<int>(std::round(seconds * 1000.0));
    const int ms = totalMs % 1000;
    const int totalSec = totalMs / 1000;
    const int s = totalSec % 60;
    const int totalMin = totalSec / 60;
    const int m = totalMin % 60;
    const int h = totalMin / 60;

    if (h > 0) {
        return QString("%1:%2:%3.%4")
            .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'))
            .arg(ms, 3, 10, QChar('0'));
    }
    return QString("%1:%2.%3")
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(ms, 3, 10, QChar('0'));
}

bool isVideoFile(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    return (ext == "mp4" || ext == "mkv" || ext == "mov" ||
            ext == "webm" || ext == "avi" || ext == "m4v" || ext == "ts");
}

} // namespace

VideoCompareWidget::VideoCompareWidget(
    MediaService& mediaService,
    InspectionScheduler& scheduler,
    QWidget* parent)
    : QWidget(parent),
      m_mediaService(mediaService),
      m_scheduler(scheduler)
{
    setAcceptDrops(true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 14, 16, 14);
    mainLayout->setSpacing(12);

    // ==================== TOP ACTION TOOLBAR ====================
    auto* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(10);

    auto* titleLabel = new QLabel("Video Compare & Media Inspector", this);
    titleLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: palette(window-text);");
    toolbarLayout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel("— Structural stream comparison and codec compatibility diagnostics", this);
    subtitleLabel->setStyleSheet("font-size: 12px; color: palette(placeholder-text);");
    toolbarLayout->addWidget(subtitleLabel, 1);

    m_swapBtn = new QPushButton("⇄ Swap A ↔ B", this);
    m_swapBtn->setCursor(Qt::PointingHandCursor);
    m_swapBtn->setStyleSheet(
        "QPushButton {"
        "    padding: 5px 12px;"
        "    font-size: 11px;"
        "    font-weight: 500;"
        "    border: 1px solid palette(mid);"
        "    border-radius: 4px;"
        "    background-color: transparent;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(13, 148, 136, 0.1);"
        "    color: #0d9488;"
        "}"
    );
    connect(m_swapBtn, &QPushButton::clicked, this, &VideoCompareWidget::swapVideos);
    toolbarLayout->addWidget(m_swapBtn);

    m_clearAllBtn = new QPushButton("Clear Both", this);
    m_clearAllBtn->setCursor(Qt::PointingHandCursor);
    m_clearAllBtn->setStyleSheet(
        "QPushButton {"
        "    padding: 5px 12px;"
        "    font-size: 11px;"
        "    font-weight: 500;"
        "    border: 1px solid palette(mid);"
        "    border-radius: 4px;"
        "    background-color: transparent;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(239, 68, 68, 0.1);"
        "    color: #ef4444;"
        "}"
    );
    connect(m_clearAllBtn, &QPushButton::clicked, this, &VideoCompareWidget::clearAll);
    toolbarLayout->addWidget(m_clearAllBtn);

    mainLayout->addLayout(toolbarLayout);

    // ==================== DUAL VIDEO PREVIEW CARDS ====================
    auto* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(14);

    QWidget* cardA = createVideoCard(
        "Video A — Reference / Baseline",
        m_previewLabelA,
        m_nameLabelA,
        m_badgeLabelA,
        m_selectBtnA,
        m_clearBtnA,
        true
    );

    QWidget* cardB = createVideoCard(
        "Video B — Comparison / Output",
        m_previewLabelB,
        m_nameLabelB,
        m_badgeLabelB,
        m_selectBtnB,
        m_clearBtnB,
        false
    );

    cardsLayout->addWidget(cardA, 1);
    cardsLayout->addWidget(cardB, 1);
    mainLayout->addLayout(cardsLayout);

    // ==================== COMPARISON TABLE ====================
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(4);
    m_tableWidget->setHorizontalHeaderLabels({
        "Stream Parameter",
        "Video A (Reference)",
        "Video B (Comparison)",
        "Analysis / Diff"
    });
    m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setShowGrid(true);
    m_tableWidget->setStyleSheet(
        "QTableWidget {"
        "    border: 1px solid palette(mid);"
        "    border-radius: 6px;"
        "    background-color: palette(base);"
        "    font-size: 11px;"
        "}"
        "QHeaderView::section {"
        "    background-color: palette(window);"
        "    font-weight: bold;"
        "    padding: 6px 10px;"
        "    border: none;"
        "    border-bottom: 2px solid #0d9488;"
        "    font-size: 11px;"
        "}"
    );

    mainLayout->addWidget(m_tableWidget, 1);

    // ==================== COMPATIBILITY & DIAGNOSTICS CARD ====================
    m_diagnosticsCard = new QFrame(this);
    m_diagnosticsCard->setStyleSheet(
        "QFrame {"
        "    border: 1px solid palette(mid);"
        "    border-radius: 6px;"
        "    background-color: rgba(128, 128, 128, 0.05);"
        "}"
    );
    auto* diagLayout = new QVBoxLayout(m_diagnosticsCard);
    diagLayout->setContentsMargins(14, 10, 14, 10);
    diagLayout->setSpacing(6);

    m_diagnosticsTitle = new QLabel("📋 Codec & Platform Compatibility Diagnostics", m_diagnosticsCard);
    m_diagnosticsTitle->setStyleSheet("font-weight: bold; font-size: 12px; color: palette(window-text);");
    diagLayout->addWidget(m_diagnosticsTitle);

    m_diagnosticsContent = new QLabel(
        "Load two video files above to analyze container atoms, stream parameters, and evaluate web/mobile playback compliance.",
        m_diagnosticsCard
    );
    m_diagnosticsContent->setTextFormat(Qt::RichText);
    m_diagnosticsContent->setStyleSheet("font-size: 11px; color: palette(placeholder-text); line-height: 1.4;");
    m_diagnosticsContent->setWordWrap(true);
    diagLayout->addWidget(m_diagnosticsContent);

    mainLayout->addWidget(m_diagnosticsCard);

    // Connect to InspectionScheduler
    connect(&m_scheduler, &InspectionScheduler::metadataReady,
            this, &VideoCompareWidget::onInspectionMetadataReady);
    connect(&m_scheduler, &InspectionScheduler::thumbnailReady,
            this, &VideoCompareWidget::onInspectionThumbnailReady);

    updateTable();
}

QWidget* VideoCompareWidget::createVideoCard(
    const QString& headerTitle,
    QLabel*& previewLabel,
    QLabel*& nameLabel,
    QLabel*& badgeLabel,
    QPushButton*& selectBtn,
    QPushButton*& clearBtn,
    bool isCardA)
{
    auto* card = new QFrame(this);
    card->setObjectName(isCardA ? "cardA" : "cardB");
    card->setStyleSheet(
        "QFrame {"
        "    border: 1px solid palette(mid);"
        "    border-radius: 8px;"
        "    background-color: rgba(128, 128, 128, 0.04);"
        "}"
    );

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    auto* topHeader = new QHBoxLayout();
    auto* heading = new QLabel(headerTitle, card);
    heading->setStyleSheet("font-weight: bold; font-size: 12px; color: #0d9488;");
    topHeader->addWidget(heading, 1);

    clearBtn = new QPushButton("✕", card);
    clearBtn->setFixedSize(20, 20);
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setToolTip("Clear this video");
    clearBtn->setStyleSheet(
        "QPushButton {"
        "    border: none;"
        "    border-radius: 10px;"
        "    background-color: rgba(128, 128, 128, 0.15);"
        "    font-weight: bold;"
        "    font-size: 10px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #ef4444;"
        "    color: white;"
        "}"
    );
    topHeader->addWidget(clearBtn);
    layout->addLayout(topHeader);

    // Preview area (thumbnail or placeholder)
    previewLabel = new QLabel(card);
    previewLabel->setFixedHeight(110);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setStyleSheet(
        "QLabel {"
        "    background-color: rgba(0, 0, 0, 0.15);"
        "    border: 1px dashed palette(mid);"
        "    border-radius: 4px;"
        "    font-size: 11px;"
        "    color: palette(placeholder-text);"
        "}"
    );
    previewLabel->setText(isCardA
        ? "Drag & drop Reference Video here\nor click Choose below"
        : "Drag & drop Comparison Video here\nor click Choose below");
    layout->addWidget(previewLabel);

    // File info labels
    nameLabel = new QLabel("No file selected", card);
    nameLabel->setStyleSheet("font-weight: 500; font-size: 11px;");
    nameLabel->setWordWrap(false);
    layout->addWidget(nameLabel);

    badgeLabel = new QLabel("—", card);
    badgeLabel->setStyleSheet("font-size: 10px; color: palette(placeholder-text);");
    layout->addWidget(badgeLabel);

    // Button row
    auto* btnRow = new QHBoxLayout();
    selectBtn = new QPushButton(isCardA ? "Choose Video A..." : "Choose Video B...", card);
    selectBtn->setCursor(Qt::PointingHandCursor);
    selectBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #0d9488;"
        "    color: white;"
        "    font-weight: bold;"
        "    padding: 5px 12px;"
        "    border-radius: 4px;"
        "    font-size: 11px;"
        "}"
        "QPushButton:hover { background-color: #0f766e; }"
    );
    btnRow->addWidget(selectBtn, 1);
    layout->addLayout(btnRow);

    if (isCardA) {
        connect(selectBtn, &QPushButton::clicked, this, &VideoCompareWidget::chooseVideoA);
        connect(clearBtn, &QPushButton::clicked, this, &VideoCompareWidget::clearVideoA);
    } else {
        connect(selectBtn, &QPushButton::clicked, this, &VideoCompareWidget::chooseVideoB);
        connect(clearBtn, &QPushButton::clicked, this, &VideoCompareWidget::clearVideoB);
    }

    return card;
}

void VideoCompareWidget::chooseVideoA()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Select Reference Video A",
        QString(),
        "Video Files (*.mp4 *.mkv *.mov *.webm *.avi *.m4v *.ts);;All Files (*.*)"
    );
    if (!path.isEmpty()) {
        loadVideoA(path);
    }
}

void VideoCompareWidget::chooseVideoB()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Select Comparison Video B",
        QString(),
        "Video Files (*.mp4 *.mkv *.mov *.webm *.avi *.m4v *.ts);;All Files (*.*)"
    );
    if (!path.isEmpty()) {
        loadVideoB(path);
    }
}

void VideoCompareWidget::loadVideoA(const QString& path)
{
    if (path.isEmpty()) return;
    m_filePathA = path;
    m_hasMetaA = false;
    m_metaA = VideoMetadata();
    m_pixmapA = QPixmap();

    const QFileInfo fi(path);
    m_nameLabelA->setText(fi.fileName());
    m_nameLabelA->setToolTip(path);
    m_badgeLabelA->setText("Probing stream metadata...");
    m_previewLabelA->setText("Loading preview...");

    // Request non-blocking metadata and thumbnail via scheduler
    m_pendingMetaIdA = m_scheduler.requestMetadata(path, "compare_meta_a", true);
    m_pendingThumbIdA = m_scheduler.requestThumbnail(path, 1.0, 320, 180, "compare_thumb_a", true);

    updateTable();
    updateDiagnostics();
}

void VideoCompareWidget::loadVideoB(const QString& path)
{
    if (path.isEmpty()) return;
    m_filePathB = path;
    m_hasMetaB = false;
    m_metaB = VideoMetadata();
    m_pixmapB = QPixmap();

    const QFileInfo fi(path);
    m_nameLabelB->setText(fi.fileName());
    m_nameLabelB->setToolTip(path);
    m_badgeLabelB->setText("Probing stream metadata...");
    m_previewLabelB->setText("Loading preview...");

    // Request non-blocking metadata and thumbnail via scheduler
    m_pendingMetaIdB = m_scheduler.requestMetadata(path, "compare_meta_b", true);
    m_pendingThumbIdB = m_scheduler.requestThumbnail(path, 1.0, 320, 180, "compare_thumb_b", true);

    updateTable();
    updateDiagnostics();
}

void VideoCompareWidget::clearVideoA()
{
    m_filePathA.clear();
    m_hasMetaA = false;
    m_metaA = VideoMetadata();
    m_pixmapA = QPixmap();
    m_pendingMetaIdA = RequestId();
    m_pendingThumbIdA = RequestId();

    m_previewLabelA->setPixmap(QPixmap());
    m_previewLabelA->setText("Drag & drop Reference Video here\nor click Choose below");
    m_nameLabelA->setText("No file selected");
    m_nameLabelA->setToolTip("");
    m_badgeLabelA->setText("—");

    updateTable();
    updateDiagnostics();
}

void VideoCompareWidget::clearVideoB()
{
    m_filePathB.clear();
    m_hasMetaB = false;
    m_metaB = VideoMetadata();
    m_pixmapB = QPixmap();
    m_pendingMetaIdB = RequestId();
    m_pendingThumbIdB = RequestId();

    m_previewLabelB->setPixmap(QPixmap());
    m_previewLabelB->setText("Drag & drop Comparison Video here\nor click Choose below");
    m_nameLabelB->setText("No file selected");
    m_nameLabelB->setToolTip("");
    m_badgeLabelB->setText("—");

    updateTable();
    updateDiagnostics();
}

void VideoCompareWidget::clearAll()
{
    clearVideoA();
    clearVideoB();
}

void VideoCompareWidget::swapVideos()
{
    if (m_filePathA.isEmpty() && m_filePathB.isEmpty()) return;

    std::swap(m_filePathA, m_filePathB);
    std::swap(m_metaA, m_metaB);
    std::swap(m_hasMetaA, m_hasMetaB);
    std::swap(m_pixmapA, m_pixmapB);
    std::swap(m_pendingMetaIdA, m_pendingMetaIdB);
    std::swap(m_pendingThumbIdA, m_pendingThumbIdB);

    // Update labels and cards
    if (m_filePathA.isEmpty()) {
        m_nameLabelA->setText("No file selected");
        m_nameLabelA->setToolTip("");
        m_badgeLabelA->setText("—");
        m_previewLabelA->setPixmap(QPixmap());
        m_previewLabelA->setText("Drag & drop Reference Video here\nor click Choose below");
    } else {
        m_nameLabelA->setText(QFileInfo(m_filePathA).fileName());
        m_nameLabelA->setToolTip(m_filePathA);
        updateThumbnailCardA();
    }

    if (m_filePathB.isEmpty()) {
        m_nameLabelB->setText("No file selected");
        m_nameLabelB->setToolTip("");
        m_badgeLabelB->setText("—");
        m_previewLabelB->setPixmap(QPixmap());
        m_previewLabelB->setText("Drag & drop Comparison Video here\nor click Choose below");
    } else {
        m_nameLabelB->setText(QFileInfo(m_filePathB).fileName());
        m_nameLabelB->setToolTip(m_filePathB);
        updateThumbnailCardB();
    }

    updateTable();
    updateDiagnostics();
}

void VideoCompareWidget::onInspectionMetadataReady(const ProbeResult& result)
{
    if (result.id == m_pendingMetaIdA) {
        if (result.success) {
            m_metaA = result.metadata;
            m_hasMetaA = true;
            updateThumbnailCardA();
        } else {
            m_badgeLabelA->setText("Error: " + result.errorMessage);
        }
        updateTable();
        updateDiagnostics();
    } else if (result.id == m_pendingMetaIdB) {
        if (result.success) {
            m_metaB = result.metadata;
            m_hasMetaB = true;
            updateThumbnailCardB();
        } else {
            m_badgeLabelB->setText("Error: " + result.errorMessage);
        }
        updateTable();
        updateDiagnostics();
    }
}

void VideoCompareWidget::onInspectionThumbnailReady(const ThumbnailResult& result)
{
    if (result.id == m_pendingThumbIdA) {
        if (result.success) {
            m_pixmapA = result.thumbnail;
            updateThumbnailCardA();
        }
    } else if (result.id == m_pendingThumbIdB) {
        if (result.success) {
            m_pixmapB = result.thumbnail;
            updateThumbnailCardB();
        }
    }
}

void VideoCompareWidget::updateThumbnailCardA()
{
    if (!m_pixmapA.isNull()) {
        const QPixmap scaled = m_pixmapA.scaled(
            m_previewLabelA->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        m_previewLabelA->setPixmap(scaled);
    }

    if (m_hasMetaA) {
        const QString res = (m_metaA.width > 0 && m_metaA.height > 0)
            ? QString("%1×%2").arg(m_metaA.width).arg(m_metaA.height) : "Unknown res";
        const QString codec = m_metaA.videoCodec.isEmpty() ? "Unknown" : m_metaA.videoCodec.toUpper();
        const QString dur = formatDuration(m_metaA.durationSeconds);
        const QString size = formatBytes(m_metaA.fileSizeBytes);
        m_badgeLabelA->setText(QString("%1 · %2 · %3 · %4").arg(res, codec, dur, size));
    }
}

void VideoCompareWidget::updateThumbnailCardB()
{
    if (!m_pixmapB.isNull()) {
        const QPixmap scaled = m_pixmapB.scaled(
            m_previewLabelB->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        m_previewLabelB->setPixmap(scaled);
    }

    if (m_hasMetaB) {
        const QString res = (m_metaB.width > 0 && m_metaB.height > 0)
            ? QString("%1×%2").arg(m_metaB.width).arg(m_metaB.height) : "Unknown res";
        const QString codec = m_metaB.videoCodec.isEmpty() ? "Unknown" : m_metaB.videoCodec.toUpper();
        const QString dur = formatDuration(m_metaB.durationSeconds);
        const QString size = formatBytes(m_metaB.fileSizeBytes);
        m_badgeLabelB->setText(QString("%1 · %2 · %3 · %4").arg(res, codec, dur, size));
    }
}

void VideoCompareWidget::updateTable()
{
    m_tableWidget->setRowCount(0);

    struct RowDef {
        QString category;
        QString name;
        QString valA;
        QString valB;
        QString diff;
        bool isHeader = false;
        bool isMatch = false;
        bool isDifferent = false;
    };

    QList<RowDef> rows;

    auto addHeader = [&](const QString& title) {
        rows.append({title, "", "", "", "", true, false, false});
    };

    auto addProp = [&](const QString& name, const QString& a, const QString& b, const QString& diffHint = QString()) {
        if (!m_hasMetaA && !m_hasMetaB) {
            rows.append({"", name, "—", "—", "—", false, false, false});
            return;
        }

        const QString dispA = m_hasMetaA ? (a.isEmpty() ? "None / N/A" : a) : "Not loaded";
        const QString dispB = m_hasMetaB ? (b.isEmpty() ? "None / N/A" : b) : "Not loaded";

        QString diff;
        bool isMatch = false;
        bool isDifferent = false;

        if (m_hasMetaA && m_hasMetaB) {
            if (dispA == dispB) {
                diff = "🟢 Match";
                isMatch = true;
            } else {
                diff = diffHint.isEmpty() ? "🟡 Different" : QString("🟡 %1").arg(diffHint);
                isDifferent = true;
            }
        } else {
            diff = "⚪ Awaiting input";
        }

        rows.append({"", name, dispA, dispB, diff, false, isMatch, isDifferent});
    };

    // ==================== 1. CONTAINER & FILE ====================
    addHeader("📁 Container & File Metadata");

    QString containerA = m_metaA.formatName;
    QString containerB = m_metaB.formatName;
    addProp("Container / Demuxer", containerA, containerB);

    QString sizeA = formatBytes(m_metaA.fileSizeBytes);
    QString sizeB = formatBytes(m_metaB.fileSizeBytes);
    QString sizeDiff;
    if (m_hasMetaA && m_hasMetaB && m_metaA.fileSizeBytes > 0 && m_metaB.fileSizeBytes > 0) {
        const double delta = static_cast<double>(m_metaB.fileSizeBytes - m_metaA.fileSizeBytes);
        const double pct = (delta / m_metaA.fileSizeBytes) * 100.0;
        if (std::abs(pct) < 0.5) {
            sizeDiff = "Identical size";
        } else if (pct < 0) {
            sizeDiff = QString("−%1% (%2)").arg(std::abs(pct), 0, 'f', 1).arg(formatBytes(std::abs(static_cast<qint64>(delta))));
        } else {
            sizeDiff = QString("+%1% (+%2)").arg(pct, 0, 'f', 1).arg(formatBytes(static_cast<qint64>(delta)));
        }
    }
    addProp("File Size", sizeA, sizeB, sizeDiff);

    QString durA = formatDuration(m_metaA.durationSeconds);
    QString durB = formatDuration(m_metaB.durationSeconds);
    QString durDiff;
    if (m_hasMetaA && m_hasMetaB) {
        const double deltaSec = m_metaB.durationSeconds - m_metaA.durationSeconds;
        if (std::abs(deltaSec) < 0.05) {
            durDiff = "Synced (±0.0s)";
        } else {
            durDiff = QString("Δ %1s").arg(deltaSec, 0, 'f', 2);
        }
    }
    addProp("Duration", durA, durB, durDiff);

    QString bitA = formatBitrate(m_metaA.overallBitrate);
    QString bitB = formatBitrate(m_metaB.overallBitrate);
    QString bitDiff;
    if (m_hasMetaA && m_hasMetaB && m_metaA.overallBitrate > 0 && m_metaB.overallBitrate > 0) {
        const double pct = ((m_metaB.overallBitrate - m_metaA.overallBitrate) / static_cast<double>(m_metaA.overallBitrate)) * 100.0;
        bitDiff = (pct < 0) ? QString("−%1% bitrate").arg(std::abs(pct), 0, 'f', 1)
                            : QString("+%1% bitrate").arg(pct, 0, 'f', 1);
    }
    addProp("Overall Bitrate", bitA, bitB, bitDiff);

    // ==================== 2. VIDEO STREAM ====================
    addHeader("🎬 Video Stream Parameters");

    QString resA = (m_metaA.width > 0 && m_metaA.height > 0) ? QString("%1×%2").arg(m_metaA.width).arg(m_metaA.height) : "";
    QString resB = (m_metaB.width > 0 && m_metaB.height > 0) ? QString("%1×%2").arg(m_metaB.width).arg(m_metaB.height) : "";
    QString resDiff;
    if (m_hasMetaA && m_hasMetaB && m_metaA.height > 0 && m_metaB.height > 0) {
        if (m_metaB.height < m_metaA.height) {
            resDiff = QString("Downscaled (%1p → %2p)").arg(m_metaA.height).arg(m_metaB.height);
        } else if (m_metaB.height > m_metaA.height) {
            resDiff = QString("Upscaled (%1p → %2p)").arg(m_metaA.height).arg(m_metaB.height);
        }
    }
    addProp("Resolution", resA, resB, resDiff);

    addProp("Display Aspect Ratio (DAR)", m_metaA.displayAspectRatio, m_metaB.displayAspectRatio);

    QString codecNameA = m_metaA.videoCodec.toUpper();
    QString codecNameB = m_metaB.videoCodec.toUpper();
    addProp("Video Codec", codecNameA, codecNameB);

    QString profA = m_metaA.videoProfile.isEmpty() ? "" : (m_metaA.videoLevel.isEmpty() ? m_metaA.videoProfile : QString("%1 @ L%2").arg(m_metaA.videoProfile, m_metaA.videoLevel));
    QString profB = m_metaB.videoProfile.isEmpty() ? "" : (m_metaB.videoLevel.isEmpty() ? m_metaB.videoProfile : QString("%1 @ L%2").arg(m_metaB.videoProfile, m_metaB.videoLevel));
    addProp("Profile & Level", profA, profB);

    addProp("Pixel Format (Chroma)", m_metaA.pixelFormat, m_metaB.pixelFormat);

    QString rangeA = m_metaA.colorRange.isEmpty() ? "" : (m_metaA.colorRange == "tv" ? "Limited (TV)" : (m_metaA.colorRange == "pc" ? "Full (PC)" : m_metaA.colorRange));
    QString rangeB = m_metaB.colorRange.isEmpty() ? "" : (m_metaB.colorRange == "tv" ? "Limited (TV)" : (m_metaB.colorRange == "pc" ? "Full (PC)" : m_metaB.colorRange));
    addProp("Color Range", rangeA, rangeB);

    addProp("Color Primaries / Space", m_metaA.colorSpace, m_metaB.colorSpace);

    QString fpsA = (m_metaA.frameRate > 0) ? QString("%1 fps").arg(m_metaA.frameRate, 0, 'f', 2) : "";
    QString fpsB = (m_metaB.frameRate > 0) ? QString("%1 fps").arg(m_metaB.frameRate, 0, 'f', 2) : "";
    addProp("Frame Rate", fpsA, fpsB);

    addProp("Video Bitrate", formatBitrate(m_metaA.videoBitrate), formatBitrate(m_metaB.videoBitrate));

    // ==================== 3. AUDIO STREAM ====================
    addHeader("🔊 Audio Stream Parameters");

    addProp("Audio Track Present", m_metaA.hasAudio ? "Yes" : "No / Silent", m_metaB.hasAudio ? "Yes" : "No / Silent");
    addProp("Audio Codec", m_metaA.audioCodec.toUpper(), m_metaB.audioCodec.toUpper());

    QString rateA = (m_metaA.audioSampleRate > 0) ? QString("%1 Hz").arg(m_metaA.audioSampleRate) : "";
    QString rateB = (m_metaB.audioSampleRate > 0) ? QString("%1 Hz").arg(m_metaB.audioSampleRate) : "";
    addProp("Sample Rate", rateA, rateB);

    QString chA = (m_metaA.audioChannels > 0) ? (m_metaA.audioChannelLayout.isEmpty() ? QString("%1 ch").arg(m_metaA.audioChannels) : QString("%1 (%2 ch)").arg(m_metaA.audioChannelLayout).arg(m_metaA.audioChannels)) : "";
    QString chB = (m_metaB.audioChannels > 0) ? (m_metaB.audioChannelLayout.isEmpty() ? QString("%1 ch").arg(m_metaB.audioChannels) : QString("%1 (%2 ch)").arg(m_metaB.audioChannelLayout).arg(m_metaB.audioChannels)) : "";
    addProp("Channels & Layout", chA, chB);

    addProp("Audio Bitrate", formatBitrate(m_metaA.audioBitrate), formatBitrate(m_metaB.audioBitrate));

    // Populate QTableWidget
    m_tableWidget->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const auto& item = rows[r];

        if (item.isHeader) {
            auto* headerItem = new QTableWidgetItem(item.category);
            headerItem->setFont(QFont(font().family(), 10, QFont::Bold));
            headerItem->setBackground(QColor(13, 148, 136, 25));
            headerItem->setForeground(QColor(13, 148, 136));
            m_tableWidget->setItem(r, 0, headerItem);
            m_tableWidget->setSpan(r, 0, 1, 4);
            continue;
        }

        auto* nameItem = new QTableWidgetItem(item.name);
        nameItem->setFont(QFont(font().family(), 9, QFont::DemiBold));

        auto* valAItem = new QTableWidgetItem(item.valA);
        auto* valBItem = new QTableWidgetItem(item.valB);

        auto* diffItem = new QTableWidgetItem(item.diff);
        if (item.isMatch) {
            diffItem->setForeground(QColor(16, 185, 129)); // Green
            diffItem->setFont(QFont(font().family(), 9, QFont::Bold));
        } else if (item.isDifferent) {
            diffItem->setForeground(QColor(217, 119, 6)); // Amber
            diffItem->setFont(QFont(font().family(), 9, QFont::Bold));
        } else {
            diffItem->setForeground(QColor(156, 163, 175)); // Grey
        }

        m_tableWidget->setItem(r, 0, nameItem);
        m_tableWidget->setItem(r, 1, valAItem);
        m_tableWidget->setItem(r, 2, valBItem);
        m_tableWidget->setItem(r, 3, diffItem);
    }
}

void VideoCompareWidget::updateDiagnostics()
{
    if (!m_hasMetaA && !m_hasMetaB) {
        m_diagnosticsTitle->setText("📋 Codec & Platform Compatibility Diagnostics");
        m_diagnosticsContent->setText(
            "Load two video files above to analyze container atoms, stream parameters, and evaluate web/mobile playback compliance."
        );
        return;
    }

    QStringList diagnostics;

    auto checkVideo = [&](const QString& label, const VideoMetadata& meta) {
        // 1. Mod-2 Dimension check
        if (meta.width > 0 && meta.height > 0) {
            if ((meta.width % 2 != 0) || (meta.height % 2 != 0)) {
                diagnostics.append(
                    QString("⚠️ <b>Mod-2 Violation in %1:</b> Dimensions %2×%3 are not divisible by 2. "
                            "H.264/HEVC compression requires even dimensions; players may introduce padding artifacts or fail hardware decoding.")
                        .arg(label).arg(meta.width).arg(meta.height)
                );
            }
        }

        // 2. Chroma Subsampling & Bit Depth
        if (!meta.pixelFormat.isEmpty() && meta.pixelFormat != "yuv420p" && meta.pixelFormat != "nv12") {
            if (meta.pixelFormat.contains("10") || meta.pixelFormat.contains("12") ||
                meta.pixelFormat.contains("422") || meta.pixelFormat.contains("444")) {
                diagnostics.append(
                    QString("⚠️ <b>High Bit Depth / Non-Standard Chroma in %1:</b> Pixel format '%2'. "
                            "10-bit and non-4:2:0 subsampling frequently fail hardware decoding or web upload on consumer platforms (e.g. WhatsApp, Discord non-Nitro, legacy TVs).")
                        .arg(label, meta.pixelFormat)
                );
            }
        }

        // 3. Profile / Level limits
        if (meta.videoCodec.toLower() == "h264") {
            if (meta.videoProfile.contains("High 10", Qt::CaseInsensitive) ||
                meta.videoProfile.contains("High 4:2:2", Qt::CaseInsensitive)) {
                diagnostics.append(
                    QString("⚠️ <b>High Profile Constraint in %1:</b> Profile '%2' is unsupported on many mobile hardware decoders.")
                        .arg(label, meta.videoProfile)
                );
            }
        }
    };

    if (m_hasMetaA) checkVideo("Video A", m_metaA);
    if (m_hasMetaB) checkVideo("Video B", m_metaB);

    // Cross-comparison diagnostics
    if (m_hasMetaA && m_hasMetaB) {
        // Missing audio comparison
        if (m_metaA.hasAudio && !m_metaB.hasAudio) {
            diagnostics.append("⚠️ <b>Audio Track Missing:</b> Video A has audio, but Video B is silent / stripped.");
        } else if (!m_metaA.hasAudio && m_metaB.hasAudio) {
            diagnostics.append("ℹ️ <b>Audio Track Added:</b> Video B contains an audio stream not present in Video A.");
        }

        // Container / Web streaming
        if (m_metaA.formatName != m_metaB.formatName) {
            diagnostics.append(
                QString("ℹ️ <b>Container Format Change:</b> Video A ('%1') vs Video B ('%2'). "
                        "MP4 with H.264/AAC remains the gold standard for universal mobile compatibility.")
                    .arg(m_metaA.formatName, m_metaB.formatName)
            );
        }

        // Compression / Size Ratio
        if (m_metaA.fileSizeBytes > 0 && m_metaB.fileSizeBytes > 0) {
            const double delta = static_cast<double>(m_metaB.fileSizeBytes - m_metaA.fileSizeBytes);
            const double pct = (delta / m_metaA.fileSizeBytes) * 100.0;
            if (pct < -2.0) {
                diagnostics.append(
                    QString("🟢 <b>File Size Reduction:</b> Video B is %1% smaller than Reference A (saved %2).")
                        .arg(std::abs(pct), 0, 'f', 1)
                        .arg(formatBytes(std::abs(static_cast<qint64>(delta))))
                );
            } else if (pct > 2.0) {
                diagnostics.append(
                    QString("🟡 <b>File Size Expansion:</b> Video B is %1% larger than Reference A (+%2).")
                        .arg(pct, 0, 'f', 1)
                        .arg(formatBytes(static_cast<qint64>(delta)))
                );
            }
        }
    }

    if (diagnostics.isEmpty()) {
        m_diagnosticsTitle->setText("✅ Compliance Diagnostics: Universal Playback Verified");
        m_diagnosticsContent->setText(
            "All inspected streams strictly conform to universal web/mobile playback standards (even mod-2 dimensions, 8-bit YUV 4:2:0 chroma, standard profile tiers, and synchronized audio)."
        );
    } else {
        m_diagnosticsTitle->setText(QString("📋 Codec & Platform Diagnostics (%1 findings)").arg(diagnostics.size()));
        m_diagnosticsContent->setText(diagnostics.join("<br><br>"));
    }
}

void VideoCompareWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl& url : urls) {
            if (isVideoFile(url.toLocalFile())) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void VideoCompareWidget::dragMoveEvent(QDragMoveEvent* event)
{
    event->acceptProposedAction();
}

void VideoCompareWidget::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    QStringList validFiles;
    for (const QUrl& url : urls) {
        const QString path = url.toLocalFile();
        if (isVideoFile(path)) {
            validFiles.append(path);
        }
    }

    if (validFiles.isEmpty()) {
        event->ignore();
        return;
    }

    // Power user feature: dropping 2 files assigns first to A and second to B
    if (validFiles.size() >= 2) {
        loadVideoA(validFiles[0]);
        loadVideoB(validFiles[1]);
        event->acceptProposedAction();
        return;
    }

    // Single file dropped: check if dropped specifically over Card A or Card B
    const QPoint dropPos = event->position().toPoint();
    QWidget* cardAWidget = findChild<QWidget*>("cardA");
    QWidget* cardBWidget = findChild<QWidget*>("cardB");

    if (cardAWidget && cardAWidget->geometry().contains(dropPos)) {
        loadVideoA(validFiles[0]);
    } else if (cardBWidget && cardBWidget->geometry().contains(dropPos)) {
        loadVideoB(validFiles[0]);
    } else {
        // Fallback: fill A first, then B, else replace B
        if (m_filePathA.isEmpty()) {
            loadVideoA(validFiles[0]);
        } else if (m_filePathB.isEmpty()) {
            loadVideoB(validFiles[0]);
        } else {
            loadVideoB(validFiles[0]);
        }
    }

    event->acceptProposedAction();
}

void VideoCompareWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateThumbnailCardA();
    updateThumbnailCardB();
}
