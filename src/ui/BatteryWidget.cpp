#include "BatteryWidget.h"

#include <QPainter>
#include <QResizeEvent>
#include <QSizePolicy>

#include "Theme.h"

namespace sc {

namespace {

// Business thresholds for the charge-level colouring (must not change).
constexpr double kErrThreshold = 15.0;
constexpr double kWarnThreshold = 30.0;

} // namespace

BatteryWidget::BatteryWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(180, 110);
    setMaximumSize(460, 220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize BatteryWidget::sizeHint() const
{
    return QSize(300, 170);
}

void BatteryWidget::setLabel(const QString &label)
{
    m_label = label;
    rebuildCache();
    update();
}

void BatteryWidget::setValue(double percent)
{
    m_value = qBound(0.0, percent, 100.0);
    update();
}

QColor BatteryWidget::fillColor() const
{
    if (m_value <= kErrThreshold)
        return QColor(QLatin1String(Theme::Err));
    if (m_value <= kWarnThreshold)
        return QColor(QLatin1String(Theme::Warn));
    return QColor(QLatin1String(Theme::Accent));
}

void BatteryWidget::resizeEvent(QResizeEvent *event)
{
    rebuildCache();
    QWidget::resizeEvent(event);
}

// Renders label, terminal nub and cell shell into the static cache and
// prepares the fill clip path. Runs on resize and setLabel() only.
void BatteryWidget::rebuildCache()
{
    m_staticLayer = QPixmap();
    if (width() < 40 || height() < 40)
        return;

    const qreal dpr = devicePixelRatioF();
    m_staticLayer = QPixmap(size() * dpr);
    m_staticLayer.setDevicePixelRatio(dpr);
    m_staticLayer.fill(Qt::transparent);

    const qreal labelH = m_label.isEmpty() ? 0.0 : height() * 0.16;
    const qreal nubW = qMax(6.0, width() * 0.045);
    const qreal nubH = height() * 0.34;
    const qreal margin = 6.0;

    m_bodyRect = QRectF(margin, labelH + margin,
                        width() - nubW - margin * 2.0,
                        height() - labelH - margin * 2.0);
    if (m_bodyRect.width() < 20.0 || m_bodyRect.height() < 20.0)
        return;

    const qreal inset = 4.0;
    m_innerRect = m_bodyRect.adjusted(inset, inset, -inset, -inset);
    const qreal radius = qMin(m_bodyRect.height() * 0.28, 16.0);
    m_innerClip = QPainterPath();
    m_innerClip.addRoundedRect(m_innerRect, radius * 0.7, radius * 0.7);

    m_valueFont = Theme::monoFont(
        qMax(16, int(qMin(m_bodyRect.height() * 0.42, m_bodyRect.width() * 0.30))),
        QFont::Bold);

    // ------------------------------------------------------- static layer
    QPainter painter(&m_staticLayer);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!m_label.isEmpty()) {
        QFont labelFont = font();
        labelFont.setPixelSize(qMax(12, int(height() * 0.12)));
        labelFont.setWeight(QFont::DemiBold);
        painter.setFont(labelFont);
        painter.setPen(QColor(QLatin1String(Theme::TextSecondary)));
        painter.drawText(QRectF(0, 0, width(), labelH), Qt::AlignCenter, m_label);
    }

    // Terminal nub on the right.
    const QRectF nub(m_bodyRect.right(), m_bodyRect.center().y() - nubH / 2.0,
                     nubW, nubH);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QLatin1String(Theme::CardBorder)));
    painter.drawRoundedRect(nub, nubW * 0.4, nubW * 0.4);

    // Cell shell.
    painter.setBrush(QColor(QLatin1String(Theme::Bg)));
    painter.setPen(QPen(QColor(QLatin1String(Theme::CardBorder)), 2.0));
    painter.drawRoundedRect(m_bodyRect, radius, radius);
}

void BatteryWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    if (m_staticLayer.isNull())
        rebuildCache();
    if (m_staticLayer.isNull())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPixmap(0, 0, m_staticLayer);

    // Charge fill, clipped to the inner cell so the corners stay clean.
    const qreal fillW = m_innerRect.width() * (m_value / 100.0);
    if (fillW > 0.5) {
        painter.save();
        painter.setClipPath(m_innerClip);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fillColor());
        painter.drawRect(QRectF(m_innerRect.left(), m_innerRect.top(),
                                fillW, m_innerRect.height()));
        painter.restore();
    }

    // Percentage readout (monospaced digits -> no width jitter).
    painter.setFont(m_valueFont);
    painter.setPen(QColor(QLatin1String(Theme::TextPrimary)));
    painter.drawText(m_bodyRect, Qt::AlignCenter,
                     QString::number(qRound(m_value)) + QStringLiteral("%"));
}

} // namespace sc
