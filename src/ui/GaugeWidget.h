#pragma once

#include <QString>
#include <QWidget>

class QPainter;

namespace sc {

// QPainter-based round gauge: gradient dial, glowing value arc, redline zone,
// ticks, numbers, tapered needle and digital readout.
// Used for speed, engine rpm and fuel level.
class GaugeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GaugeWidget(QWidget *parent = nullptr);

    void configure(double minValue, double maxValue, const QString &unit,
                   double redline = -1.0, int majorTicks = 10, int precision = 0);
    void setLabel(const QString &label);
    void setValue(double value);

    double value() const { return m_value; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QRectF dialRect() const;
    double valueFraction() const;
    double valueAngleDeg() const; // 225 (bottom-left) .. -45 (bottom-right)
    void drawTicks(QPainter &painter, const QRectF &dial, const QRectF &face);
    void drawNumbers(QPainter &painter, const QRectF &dial, const QRectF &face);
    void drawNeedle(QPainter &painter, qreal cx, qreal cy, qreal r, double angleDeg);
    void drawValue(QPainter &painter, qreal cx, qreal cy, qreal r);

    double m_min = 0.0;
    double m_max = 240.0;
    double m_value = 0.0;
    double m_redline = -1.0;
    int m_majorTicks = 10;
    int m_precision = 0;
    QString m_unit = QStringLiteral("km/h");
    QString m_label;
};

} // namespace sc
