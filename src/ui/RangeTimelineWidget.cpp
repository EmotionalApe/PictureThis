#include "RangeTimelineWidget.h"

#include <algorithm>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>

RangeTimelineWidget::RangeTimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(42);
}

void RangeTimelineWidget::setRange(double startTime, double endTime, double totalDuration)
{
    m_startTime = std::max(0.0, startTime);
    m_totalDuration = std::max(0.0, totalDuration);
    m_endTime = std::clamp(endTime, m_startTime, m_totalDuration > 0.0 ? m_totalDuration : m_startTime);
    update();
}

void RangeTimelineWidget::setTimeRange(double startTime, double endTime)
{
    m_startTime = std::max(0.0, startTime);
    m_endTime = std::clamp(endTime, m_startTime, m_totalDuration > 0.0 ? m_totalDuration : m_startTime);
    update();
}

QSize RangeTimelineWidget::sizeHint() const
{
    return QSize(300, 42);
}

QString RangeTimelineWidget::formatTime(double seconds)
{
    const int totalSec = static_cast<int>(seconds + 0.5);
    const int hrs = totalSec / 3600;
    const int mins = (totalSec % 3600) / 60;
    const int secs = totalSec % 60;

    if (hrs > 0) {
        return QString::asprintf("%02d:%02d:%02d", hrs, mins, secs);
    }
    return QString::asprintf("%02d:%02d", mins, secs);
}

void RangeTimelineWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();

    const int trackMarginH = 8;
    const int trackY = 8;
    const int trackHeight = 10;
    const int trackWidth = std::max(10, w - 2 * trackMarginH);

    // 1. Draw track background
    const QRectF trackRect(trackMarginH, trackY, trackWidth, trackHeight);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(220, 224, 230)); // Subtle light-gray
    painter.drawRoundedRect(trackRect, 5.0, 5.0);

    // 2. Draw active highlighted range
    if (m_totalDuration > 0.0 && m_endTime > m_startTime) {
        const double startRatio = std::clamp(m_startTime / m_totalDuration, 0.0, 1.0);
        const double endRatio = std::clamp(m_endTime / m_totalDuration, 0.0, 1.0);

        const double activeX = trackMarginH + startRatio * trackWidth;
        const double activeW = std::max(3.0, (endRatio - startRatio) * trackWidth);

        const QRectF activeRect(activeX, trackY, activeW, trackHeight);
        painter.setBrush(QColor(59, 130, 246)); // Vibrant blue (#3b82f6)
        painter.drawRoundedRect(activeRect, 5.0, 5.0);

        // Draw start and end pin handles
        painter.setPen(QPen(QColor(37, 99, 235), 2));
        painter.setBrush(Qt::white);

        const int handleRadius = 6;
        painter.drawEllipse(QPointF(activeX, trackY + trackHeight / 2.0), handleRadius, handleRadius);
        painter.drawEllipse(QPointF(activeX + activeW, trackY + trackHeight / 2.0), handleRadius, handleRadius);
    }

    // 3. Draw text markers below track
    painter.setPen(QColor(100, 116, 139));
    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);

    const int textY = trackY + trackHeight + 16;

    // Left label: Start time
    const QString leftStr = formatTime(m_startTime);
    painter.drawText(trackMarginH, textY, leftStr);

    // Right label: End / Total time
    const QString rightStr = m_totalDuration > 0.0 ? formatTime(m_totalDuration) : "00:00";
    const int rightW = painter.fontMetrics().horizontalAdvance(rightStr);
    painter.drawText(w - trackMarginH - rightW, textY, rightStr);

    // Center label: Cut duration
    if (m_totalDuration > 0.0 && m_endTime > m_startTime) {
        const QString centerStr = QString("Cut: %1").arg(formatTime(m_endTime - m_startTime));
        const int centerW = painter.fontMetrics().horizontalAdvance(centerStr);
        const int centerX = (w - centerW) / 2;
        painter.setPen(QColor(30, 41, 59));
        painter.drawText(centerX, textY, centerStr);
    }
}
