#pragma once

#include <QWidget>

class RangeTimelineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RangeTimelineWidget(QWidget* parent = nullptr);
    ~RangeTimelineWidget() override = default;

    void setRange(double startTime, double endTime, double totalDuration);
    void setTimeRange(double startTime, double endTime);

    double startTime() const { return m_startTime; }
    double endTime() const { return m_endTime; }
    double totalDuration() const { return m_totalDuration; }

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;

private:
    static QString formatTime(double seconds);

    double m_totalDuration = 0.0;
    double m_startTime = 0.0;
    double m_endTime = 0.0;
};
