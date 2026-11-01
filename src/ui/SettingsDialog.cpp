#include "SettingsDialog.h"
#include "../media/MediaService.h"
#include "../media/ExecutableLocator.h"

#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

SettingsDialog::SettingsDialog(
    MediaService& mediaService,
    QWidget* parent)
    : QDialog(parent),
      m_mediaService(mediaService)
{
    setWindowTitle("Media Tools & Preferences");
    resize(640, 380);
    setMinimumWidth(560);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Header description
    auto* titleLabel = new QLabel("<b>External Media Tools (FFmpeg & FFprobe)</b>", this);
    titleLabel->setStyleSheet("font-size: 14px;");
    auto* descLabel = new QLabel(
        "Picture This uses FFmpeg and FFprobe to perform all media conversions and probes.<br>"
        "You can configure explicit binary paths or let the application auto-discover them.",
        this
    );
    descLabel->setStyleSheet("color: palette(placeholder-text); font-size: 11px;");
    descLabel->setWordWrap(true);

    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(descLabel);

    // Group box for paths
    auto* toolsGroup = new QGroupBox("Tool Executables", this);
    auto* toolsLayout = new QVBoxLayout(toolsGroup);
    toolsLayout->setSpacing(14);

    // 1. FFmpeg Row
    auto* ffmpegTitle = new QLabel("<b>FFmpeg Executable</b>", this);
    auto* ffmpegRow = new QHBoxLayout();
    m_ffmpegPathEdit = new QLineEdit(this);
    m_ffmpegPathEdit->setPlaceholderText("Auto-detected via PATH or application folder");
    m_ffmpegBrowseBtn = new QPushButton("Browse...", this);
    m_ffmpegTestBtn = new QPushButton("Test", this);

    ffmpegRow->addWidget(m_ffmpegPathEdit, 1);
    ffmpegRow->addWidget(m_ffmpegBrowseBtn);
    ffmpegRow->addWidget(m_ffmpegTestBtn);

    m_ffmpegStatusLabel = new QLabel(this);
    m_ffmpegStatusLabel->setWordWrap(true);
    m_ffmpegStatusLabel->setStyleSheet("font-size: 11px;");

    toolsLayout->addWidget(ffmpegTitle);
    toolsLayout->addLayout(ffmpegRow);
    toolsLayout->addWidget(m_ffmpegStatusLabel);

    // Divider line
    auto* divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
    toolsLayout->addWidget(divider);

    // 2. FFprobe Row
    auto* ffprobeTitle = new QLabel("<b>FFprobe Executable</b>", this);
    auto* ffprobeRow = new QHBoxLayout();
    m_ffprobePathEdit = new QLineEdit(this);
    m_ffprobePathEdit->setPlaceholderText("Auto-detected via PATH or application folder");
    m_ffprobeBrowseBtn = new QPushButton("Browse...", this);
    m_ffprobeTestBtn = new QPushButton("Test", this);

    ffprobeRow->addWidget(m_ffprobePathEdit, 1);
    ffprobeRow->addWidget(m_ffprobeBrowseBtn);
    ffprobeRow->addWidget(m_ffprobeTestBtn);

    m_ffprobeStatusLabel = new QLabel(this);
    m_ffprobeStatusLabel->setWordWrap(true);
    m_ffprobeStatusLabel->setStyleSheet("font-size: 11px;");

    toolsLayout->addWidget(ffprobeTitle);
    toolsLayout->addLayout(ffprobeRow);
    toolsLayout->addWidget(m_ffprobeStatusLabel);

    mainLayout->addWidget(toolsGroup);

    // Actions & Buttons
    auto* bottomRow = new QHBoxLayout();

    m_autoDetectBtn = new QPushButton("Auto-Detect / Reset", this);
    m_autoDetectBtn->setToolTip("Clear custom paths and auto-discover tools in system PATH or app directory");

    m_saveBtn = new QPushButton("Save Preferences", this);
    m_saveBtn->setDefault(true);
    m_saveBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #0d9488;"
        "    color: white;"
        "    font-weight: bold;"
        "    padding: 6px 16px;"
        "    border-radius: 4px;"
        "}"
        "QPushButton:hover { background-color: #0f766e; }"
    );

    m_cancelBtn = new QPushButton("Cancel", this);

    bottomRow->addWidget(m_autoDetectBtn);
    bottomRow->addStretch();
    bottomRow->addWidget(m_cancelBtn);
    bottomRow->addWidget(m_saveBtn);

    mainLayout->addLayout(bottomRow);

    // Wire events
    connect(m_ffmpegBrowseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseFfmpeg);
    connect(m_ffprobeBrowseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseFfprobe);
    connect(m_ffmpegTestBtn, &QPushButton::clicked, this, &SettingsDialog::onTestFfmpeg);
    connect(m_ffprobeTestBtn, &QPushButton::clicked, this, &SettingsDialog::onTestFfprobe);
    connect(m_autoDetectBtn, &QPushButton::clicked, this, &SettingsDialog::onAutoDetect);
    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSave);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    loadCurrentSettings();
}

void SettingsDialog::loadCurrentSettings()
{
    const QString savedFfmpeg = ExecutableLocator::savedCustomPath("ffmpeg");
    const QString savedFfprobe = ExecutableLocator::savedCustomPath("ffprobe");

    const ToolStatus autoFfmpeg = ExecutableLocator::locate("ffmpeg");
    const ToolStatus autoFfprobe = ExecutableLocator::locate("ffprobe");

    if (autoFfmpeg.isValid) {
        m_ffmpegPathEdit->setPlaceholderText(QString("Auto-detected: %1").arg(autoFfmpeg.executablePath));
    } else {
        m_ffmpegPathEdit->setPlaceholderText("Not found on system (specify custom path)");
    }

    if (autoFfprobe.isValid) {
        m_ffprobePathEdit->setPlaceholderText(QString("Auto-detected: %1").arg(autoFfprobe.executablePath));
    } else {
        m_ffprobePathEdit->setPlaceholderText("Not found on system (specify custom path)");
    }

    m_ffmpegPathEdit->setText(savedFfmpeg);
    m_ffprobePathEdit->setText(savedFfprobe);

    onTestFfmpeg();
    onTestFfprobe();
}

void SettingsDialog::onBrowseFfmpeg()
{
#ifdef Q_OS_WIN
    const QString filter = "Executables (*.exe);;All Files (*.*)";
#else
    const QString filter = "All Files (*)";
#endif
    const QString selected = QFileDialog::getOpenFileName(
        this,
        "Select FFmpeg Executable",
        m_ffmpegPathEdit->text(),
        filter
    );

    if (!selected.isEmpty()) {
        m_ffmpegPathEdit->setText(selected);
        onTestFfmpeg();
    }
}

void SettingsDialog::onBrowseFfprobe()
{
#ifdef Q_OS_WIN
    const QString filter = "Executables (*.exe);;All Files (*.*)";
#else
    const QString filter = "All Files (*)";
#endif
    const QString selected = QFileDialog::getOpenFileName(
        this,
        "Select FFprobe Executable",
        m_ffprobePathEdit->text(),
        filter
    );

    if (!selected.isEmpty()) {
        m_ffprobePathEdit->setText(selected);
        onTestFfprobe();
    }
}

void SettingsDialog::onTestFfmpeg()
{
    testExecutable(m_ffmpegPathEdit->text(), m_ffmpegStatusLabel, "ffmpeg");
}

void SettingsDialog::onTestFfprobe()
{
    testExecutable(m_ffprobePathEdit->text(), m_ffprobeStatusLabel, "ffprobe");
}

void SettingsDialog::testExecutable(const QString& path, QLabel* statusLabel, const QString& expectedTool)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        const ToolStatus autoStatus = ExecutableLocator::locate(expectedTool);
        if (autoStatus.isValid) {
            statusLabel->setText(
                QString("<span style='color: #16a34a; font-weight: bold;'>✓ Auto-detected:</span> <span style='font-family: monospace;'>%1</span>")
                    .arg(autoStatus.versionString.toHtmlEscaped())
            );
        } else {
            statusLabel->setText(
                QString("<span style='color: #dc2626; font-weight: bold;'>✗ Missing:</span> '%1' was not found in PATH or application folder.")
                    .arg(expectedTool)
            );
        }
        return;
    }

    QString version;
    const bool valid = ExecutableLocator::validate(trimmed, &version);
    if (valid) {
        statusLabel->setText(
            QString("<span style='color: #16a34a; font-weight: bold;'>✓ Valid:</span> <span style='font-family: monospace;'>%1</span>")
                .arg(version.toHtmlEscaped())
        );
    } else {
        statusLabel->setText(
            QString("<span style='color: #dc2626; font-weight: bold;'>✗ Invalid or inaccessible:</span> Failed to execute '%1 -version'.")
                .arg(expectedTool)
        );
    }
}

void SettingsDialog::onAutoDetect()
{
    ExecutableLocator::clearSavedCustomPath("ffmpeg");
    ExecutableLocator::clearSavedCustomPath("ffprobe");

    m_ffmpegPathEdit->clear();
    m_ffprobePathEdit->clear();

    loadCurrentSettings();
}

void SettingsDialog::onSave()
{
    const QString ffmpegPath = m_ffmpegPathEdit->text().trimmed();
    const QString ffprobePath = m_ffprobePathEdit->text().trimmed();

    // Check validity if paths are specified
    if (!ffmpegPath.isEmpty()) {
        if (!ExecutableLocator::validate(ffmpegPath)) {
            QMessageBox::warning(
                this,
                "Invalid FFmpeg",
                "The specified FFmpeg executable could not be executed or is invalid.\nPlease verify the path before saving."
            );
            return;
        }
    }

    if (!ffprobePath.isEmpty()) {
        if (!ExecutableLocator::validate(ffprobePath)) {
            QMessageBox::warning(
                this,
                "Invalid FFprobe",
                "The specified FFprobe executable could not be executed or is invalid.\nPlease verify the path before saving."
            );
            return;
        }
    }

    // Save custom preferences
    ExecutableLocator::setSavedCustomPath("ffmpeg", ffmpegPath);
    ExecutableLocator::setSavedCustomPath("ffprobe", ffprobePath);

    // Resolve active paths
    const ToolStatus activeFfmpeg = ExecutableLocator::locateFfmpeg();
    const ToolStatus activeFfprobe = ExecutableLocator::locateFfprobe();

    if (activeFfmpeg.isValid) {
        m_mediaService.setFfmpegExecutable(activeFfmpeg.executablePath);
    }
    if (activeFfprobe.isValid) {
        m_mediaService.setFfprobeExecutable(activeFfprobe.executablePath);
    }

    emit settingsSaved();
    accept();
}
