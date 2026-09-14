#include "GaugeWidget.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtMath>

namespace sc {

GaugeWidget::GaugeWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(180, 180);
}

void GaugeWidget::configure(double minValue, double maxValue, const QString &unit,
                            double redline, int majorTicks, int precision)
{
    m_min = minValue;
    m_max = maxValue;
    m_unit = unit;
    m_redline = redline;
    m_majorTicks = majorTicks;
    m_precision = precision;
    update();
}

void GaugeWidget::setLabel(const QString &label)
{
    m_label = label;
    update();
}

void GaugeWidget::setValue(double value)
{
    m_value = qBound(m_min, value, m_max);
    update();
}

QRectF GaugeWidget::dialRect() const
{
    const qreal side = qMin(width(), height()) - 8.0;
    return QRectF((width() - side) / 2.0, (height() - side) / 2.0, side, side);
}

double GaugeWidget::valueFraction() const
{
    return (m_value - m_min) / qMax(1.0, m_max - m_min);
}

double GaugeWidget::valueAngleDeg() const
{
    return 225.0 - valueFraction() * 270.0;
}

void GaugeWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF dial = dialRect();
    const qreal cx = dial.center().x();
    const qreal cy = dial.center().y();
    const qreal r = dial.width() / 2.0;

    // Outer rim with a subtle radial gradient ring.
    QRadialGradient rim(cx, cy, r);
    rim.setColorAt(0.0, QColor(0x23, 0x2D, 0x40));
    rim.setColorAt(0.7, QColor(0x16, 0x1C, 0x2A));
    rim.setColorAt(1.0, QColor(0x0A, 0x0D, 0x14));
    painter.setPen(Qt::NoPen);
    painter.setBrush(rim);
    painter.drawEllipse(dial);

    // Inner face.
    const QRectF face = dial.adjusted(r * 0.14, r * 0.14, -r * 0.14, -r * 0.14);
    painter.setPen(QPen(QColor(0x2A, 0x35, 0x50), 1.2));
    painter.setBrush(QColor(0x0C, 0x11, 0x1B));
    painter.drawEllipse(face);

    // Arc track.
    const qreal arcR = face.width() / 2.0 - r * 0.10;
    const QRectF arcRect(cx - arcR, cy - arcR, 2.0 * arcR, 2.0 * arcR);
    const qreal penW = r * 0.095;

    painter.setPen(QPen(QColor(0x22, 0x2B, 0x3D), penW, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(arcRect, 225 * 16, -270 * 16);

    // Redline zone.
    if (m_redline > m_min && m_redline < m_max) {
        const double redStart = 225.0 - (m_redline - m_min) / (m_max - m_min) * 270.0;
        painter.setPen(QPen(QColor(0xFF, 0x4D, 0x4F, 220), penW, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(arcRect, qRound(redStart * 16), qRound((-45.0 - redStart) * 16));
    }

    // Active value arc (neon glow underneath a solid accent).
    if (m_value > m_min) {
        const int span = qRound((valueAngleDeg() - 225.0) * 16);
        const QColor accent = (m_redline > 0 && m_value >= m_redline)
                                  ? QColor(0xFF, 0x4D, 0x4F)
                                  : QColor(0x29, 0xD3, 0xF0);
        painter.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), 50),
                            penW * 1.9, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(arcRect, 225 * 16, span);
        painter.setPen(QPen(accent, penW, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(arcRect, 225 * 16, span);
    }

    drawTicks(painter, dial, face);
    drawNumbers(painter, dial, face);
    drawNeedle(painter, cx, cy, r, valueAngleDeg());
    drawValue(painter, cx, cy, r);
}

void GaugeWidget::drawTicks(QPainter &painter, const QRectF &dial, const QRectF &face)
{
    const qreal cx = dial.center().x();
    const qreal cy = dial.center().y();
    const qreal r = dial.width() / 2.0;
    const qreal outer = face.width() / 2.0 - r * 0.06;
    const qreal majorLen = r * 0.11;
    const qreal minorLen = r * 0.055;

    const int major = m_majorTicks;
    const int minor = major * 2;

    for (int i = 0; i <= minor; ++i) {
        const double frac = double(i) / minor;
        const double angleRad = qDegreesToRadians(225.0 - frac * 270.0);
        const bool isMajor = (i % 2 == 0);
        const qreal len = isMajor ? majorLen : minorLen;
        painter.setPen(QPen(isMajor ? QColor(0xC7, 0xD0, 0xE0)
                                    : QColor(0x3A, 0x4A, 0x70, 180),
                            isMajor ? 2.0 : 1.0));
        painter.drawLine(QPointF(cx + qCos(angleRad) * (outer - len),
                                 cy - qSin(angleRad) * (outer - len)),
                         QPointF(cx + qCos(angleRad) * outer,
                                 cy - qSin(angleRad) * outer));
    }
}

void GaugeWidget::drawNumbers(QPainter &painter, const QRectF &dial, const QRectF &face)
{
    const qreal cx = dial.center().x();
    const qreal cy = dial.center().y();
    const qreal r = dial.width() / 2.0;
    const qreal radius = face.width() / 2.0 - r * 0.22;

    QFont f = font();
    f.setPixelSize(qMax(9, int(r * 0.08)));
    f.setWeight(QFont::DemiBold);
    painter.setFont(f);
    painter.setPen(QColor(0x9A, 0xA8, 0xC0));

    const QFontMetricsF metrics(f);
    const double step = (m_max - m_min) / m_majorTicks;
    for (int i = 0; i <= m_majorTicks; i += 2) {
        const double value = m_min + i * step;
        const double angleRad = qDegreesToRadians(225.0 - double(i) / m_majorTicks * 270.0);
        const qreal x = cx + qCos(angleRad) * radius;
        const qreal y = cy - qSin(angleRad) * radius;
        const QString text = QString::number(value, 'f', m_precision);

        QRectF textRect = metrics.boundingRect(text);
        textRect.moveCenter(QPointF(x, y));
        painter.drawText(textRect, Qt::AlignCenter, text);
    }
}

void GaugeWidget::drawNeedle(QPainter &painter, qreal cx, qreal cy, qreal r, double angleDeg)
{
    const qreal angleRad = qDegreesToRadians(angleDeg);
    const qreal len = r * 0.58;
    const qreal baseHalf = r * 0.045;
    const qreal baseY = cy + r * 0.30;

    const QColor color = (m_redline > 0 && m_value >= m_redline)
                             ? QColor(0xFF, 0x4D, 0x4F)
                             : QColor(0x7F, 0xE6, 0xFF);

    QPainterPath needle;
    needle.moveTo(cx - baseHalf, baseY);
    needle.lineTo(cx + baseHalf, baseY);
    needle.lineTo(cx + qCos(angleRad) * len, cy - qSin(angleRad) * len);
    needle.closeSubpath();

    QLinearGradient grad(cx, baseY, cx, cy - len);
    grad.setColorAt(0.0, color);
    grad.setColorAt(1.0, QColor(0x29, 0xD3, 0xF0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(grad);
    painter.drawPath(needle);

    // Center hub.
    QRadialGradient hub(cx, cy, r * 0.09);
    hub.setColorAt(0.0, QColor(0xEA, 0xF0, 0xF8));
    hub.setColorAt(0.6, color);
    hub.setColorAt(1.0, QColor(0x0A, 0x0D, 0x14));
    painter.setBrush(hub);
    painter.drawEllipse(QPointF(cx, cy), r * 0.075, r * 0.075);
}

void GaugeWidget::drawValue(QPainter &painter, qreal cx, qreal cy, qreal r)
{
    if (!m_label.isEmpty()) {
        QFont labelFont = font();
        labelFont.setPixelSize(qMax(10, int(r * 0.15)));
        labelFont.setWeight(QFont::Bold);
        painter.setFont(labelFont);
        painter.setPen(QColor(0x29, 0xD3, 0xF0));
        painter.drawText(QRectF(cx - r, cy - r * 0.60, 2.0 * r, r * 0.22),
                         Qt::AlignCenter, m_label);
    }

    QFont valueFont = font();
    valueFont.setPixelSize(qMax(15, int(r * 0.27)));
    valueFont.setWeight(QFont::Bold);
    painter.setFont(valueFont);
    painter.setPen(QColor(0xEA, 0xF0, 0xF8));
    painter.drawText(QRectF(cx - r, cy + r * 0.38, 2.0 * r, r * 0.32),
                     Qt::AlignCenter, QString::number(m_value, 'f', m_precision));

    QFont unitFont = font();
    unitFont.setPixelSize(qMax(10, int(r * 0.12)));
    unitFont.setWeight(QFont::DemiBold);
    painter.setFont(unitFont);
    painter.setPen(QColor(0x8A, 0x94, 0xA8));
    painter.drawText(QRectF(cx - r, cy + r * 0.70, 2.0 * r, r * 0.18),
                     Qt::AlignCenter, m_unit.toUpper());
}

} // namespace sc
