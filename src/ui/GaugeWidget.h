#pragma once

#include <QFont>
#include <QPainterPath>
#include <QPixmap>
#include <QString>
#include <QWidget>

class QPainter;

namespace sc {

// QPainter-based round gauge with a cached static layer.
//
// Everything that does not depend on the current value (dial face, arc track,
// redline zone, ticks, numbers, label) is rendered once into m_staticLayer
// whenever the widget is resized or reconfigured. Each paint then only blits
// the cache and draws the value arc, the (pre-built, rotated) needle and the
// digital readout -- no gradients, paths or font metrics per frame.
// Used for speed and engine rpm.
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
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuildCache();
    double valueFraction() const;
    double valueAngleDeg() const; // 225 (bottom-left) .. -45 (bottom-right)
    QColor valueColor() const;

    // Configuration.
    double m_min = 0.0;
    double m_max = 240.0;
    double m_value = 0.0;
    double m_redline = -1.0;
    int m_majorTicks = 10;
    int m_precision = 0;
    QString m_unit = QStringLiteral("km/h");
    QString m_label;

    // Cached static layer and geometry (rebuilt on resize/configure).
    QPixmap m_staticLayer;
    QRectF m_arcRect;
    QPointF m_center;
    qreal m_radius = 0.0;
    qreal m_arcPenWidth = 1.0;
    QPainterPath m_needlePath; // points straight up, rotated per frame
    QFont m_valueFont;
    QFont m_unitFont;
    QRectF m_valueRect;
    QRectF m_unitRect;
};

} // namespace sc
