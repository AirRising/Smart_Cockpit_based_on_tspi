#include "GaugeWidget.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QResizeEvent>
#include <QtMath>

#include "Theme.h"

namespace sc {

namespace {

constexpr double kStartAngleDeg = 225.0; // bottom-left
constexpr double kEndAngleDeg = -45.0;   // bottom-right
constexpr double kSweepDeg = 270.0;

} // namespace

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
    rebuildCache();
    update();
}

void GaugeWidget::setLabel(const QString &label)
{
    m_label = label;
    rebuildCache();
    update();
}

void GaugeWidget::setValue(double value)
{
    m_value = qBound(m_min, value, m_max);
    update();
}

double GaugeWidget::valueFraction() const
{
    return (m_value - m_min) / qMax(1.0, m_max - m_min);
}

double GaugeWidget::valueAngleDeg() const
{
    return kStartAngleDeg - valueFraction() * kSweepDeg;
}

QColor GaugeWidget::valueColor() const
{
    if (m_redline > m_min && m_value >= m_redline)
        return QColor(QLatin1String(Theme::Err));
    return QColor(QLatin1String(Theme::Accent));
}

void GaugeWidget::resizeEvent(QResizeEvent *event)
{
    rebuildCache();
    QWidget::resizeEvent(event);
}

// Renders the whole static layer (face, track, redline, ticks, numbers,
// label) plus all per-frame geometry into members. Runs on resize and on
// configure()/setLabel() only -- never per value change.
void GaugeWidget::rebuildCache()
{
    m_staticLayer = QPixmap();
    if (width() < 40 || height() < 40)
        return;

    const qreal dpr = devicePixelRatioF();
    m_staticLayer = QPixmap(size() * dpr);
    m_staticLayer.setDevicePixelRatio(dpr);
    m_staticLayer.fill(Qt::transparent);

    // Geometry shared by the static layer and the per-frame drawing.
    const qreal side = qMin<qreal>(width(), height()) - 16.0;
    const QRectF dial((width() - side) / 2.0, (height() - side) / 2.0, side, side);
    m_center = dial.center();
    m_radius = dial.width() / 2.0;
    const qreal cx = m_center.x();
    const qreal cy = m_center.y();
    const qreal r = m_radius;

    const QRectF face = dial.adjusted(r * 0.10, r * 0.10, -r * 0.10, -r * 0.10);
    m_arcPenWidth = qMax(4.0, r * 0.055);
    const qreal arcR = face.width() / 2.0 - m_arcPenWidth / 2.0 - 2.0;
    m_arcRect = QRectF(cx - arcR, cy - arcR, 2.0 * arcR, 2.0 * arcR);

    // Needle path points straight up; paintEvent translates to the hub and
    // rotates it into place, so no path is ever built per frame.
    const qreal needleLen = arcR - m_arcPenWidth * 1.2;
    const qreal baseHalf = qMax(2.0, r * 0.035);
    m_needlePath = QPainterPath();
    m_needlePath.moveTo(-baseHalf, r * 0.12);
    m_needlePath.lineTo(baseHalf, r * 0.12);
    m_needlePath.lineTo(0.0, -needleLen);
    m_needlePath.closeSubpath();

    // Readout fonts and target rects (content is drawn per frame).
    m_valueFont = Theme::monoFont(qMax(18, int(r * 0.30)), QFont::Bold);
    m_unitFont = Theme::monoFont(qMax(10, int(r * 0.10)), QFont::DemiBold);
    m_valueRect = QRectF(cx - r, cy + r * 0.10, 2.0 * r, r * 0.40);
    m_unitRect = QRectF(cx - r, cy + r * 0.52, 2.0 * r, r * 0.18);

    // ------------------------------------------------------- static layer
    QPainter painter(&m_staticLayer);
    painter.setRenderHint(QPainter::Antialiasing);

    // Dial face.
    painter.setPen(QPen(QColor(QLatin1String(Theme::CardBorder)), 1.2));
    painter.setBrush(QColor(QLatin1String(Theme::Card)));
    painter.drawEllipse(face);

    // Arc track.
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(QLatin1String(Theme::CardBorder)), m_arcPenWidth,
                        Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(m_arcRect, int(kStartAngleDeg * 16), int(-kSweepDeg * 16));

    // Redline zone.
    if (m_redline > m_min && m_redline < m_max) {
        const double redStart =
            kStartAngleDeg - (m_redline - m_min) / (m_max - m_min) * kSweepDeg;
        painter.setPen(QPen(QColor(QLatin1String(Theme::Err)), m_arcPenWidth,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(m_arcRect, qRound(redStart * 16),
                        qRound((kEndAngleDeg - redStart) * 16));
    }

    // Ticks (inside the arc).
    const qreal tickOuter = arcR - m_arcPenWidth - 2.0;
    const qreal majorLen = r * 0.07;
    const qreal minorLen = r * 0.035;
    const int minor = m_majorTicks * 2;
    for (int i = 0; i <= minor; ++i) {
        const double angleRad =
            qDegreesToRadians(kStartAngleDeg - double(i) / minor * kSweepDeg);
        const bool isMajor = (i % 2 == 0);
        const qreal len = isMajor ? majorLen : minorLen;
        painter.setPen(QPen(isMajor ? QColor(QLatin1String(Theme::TextSecondary))
                                    : QColor(QLatin1String(Theme::Disabled)),
                            isMajor ? 2.0 : 1.0));
        painter.drawLine(QPointF(cx + qCos(angleRad) * (tickOuter - len),
                                 cy - qSin(angleRad) * (tickOuter - len)),
                         QPointF(cx + qCos(angleRad) * tickOuter,
                                 cy - qSin(angleRad) * tickOuter));
    }

    // Numbers (every other major tick, like before).
    QFont numberFont = Theme::monoFont(qMax(9, int(r * 0.075)), QFont::DemiBold);
    painter.setFont(numberFont);
    painter.setPen(QColor(QLatin1String(Theme::TextSecondary)));
    const QFontMetricsF metrics(numberFont);
    const qreal numberRadius = tickOuter - majorLen - r * 0.10;
    const double step = (m_max - m_min) / m_majorTicks;
    for (int i = 0; i <= m_majorTicks; i += 2) {
        const double value = m_min + i * step;
        const double angleRad = qDegreesToRadians(
            kStartAngleDeg - double(i) / m_majorTicks * kSweepDeg);
        const QString text = QString::number(value, 'f', m_precision);
        QRectF textRect = metrics.boundingRect(text);
        textRect.moveCenter(QPointF(cx + qCos(angleRad) * numberRadius,
                                    cy - qSin(angleRad) * numberRadius));
        painter.drawText(textRect, Qt::AlignCenter, text);
    }

    // Label above the hub.
    if (!m_label.isEmpty()) {
        QFont labelFont = font();
        labelFont.setPixelSize(qMax(11, int(r * 0.11)));
        labelFont.setWeight(QFont::DemiBold);
        painter.setFont(labelFont);
        painter.setPen(QColor(QLatin1String(Theme::TextSecondary)));
        painter.drawText(QRectF(cx - r, cy - r * 0.62, 2.0 * r, r * 0.20),
                         Qt::AlignCenter, m_label);
    }
}

void GaugeWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    if (m_staticLayer.isNull())
        rebuildCache();
    if (m_staticLayer.isNull())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPixmap(0, 0, m_staticLayer);

    const QColor color = valueColor();

    // Value arc.
    if (m_value > m_min) {
        const int span = qRound((valueAngleDeg() - kStartAngleDeg) * 16.0);
        painter.setPen(QPen(color, m_arcPenWidth, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(m_arcRect, int(kStartAngleDeg * 16), span);
    }

    // Needle: pre-built path, only translate + rotate per frame.
    painter.save();
    painter.translate(m_center);
    painter.rotate(90.0 - valueAngleDeg());
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPath(m_needlePath);
    painter.restore();

    // Hub.
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QLatin1String(Theme::TextPrimary)));
    painter.drawEllipse(m_center, m_radius * 0.035, m_radius * 0.035);

    // Digital readout (monospaced digits -> no width jitter).
    painter.setFont(m_valueFont);
    painter.setPen(QColor(QLatin1String(Theme::TextPrimary)));
    painter.drawText(m_valueRect, Qt::AlignCenter,
                     QString::number(m_value, 'f', m_precision));

    painter.setFont(m_unitFont);
    painter.setPen(QColor(QLatin1String(Theme::TextSecondary)));
    painter.drawText(m_unitRect, Qt::AlignCenter, m_unit.toUpper());
}

} // namespace sc
