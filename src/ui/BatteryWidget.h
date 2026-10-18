#pragma once

#include <QColor>
#include <QFont>
#include <QPainterPath>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QWidget>

class QPainter;

namespace sc {

// QPainter-based horizontal battery indicator with a cached static layer:
// the cell shell, terminal nub and label are rendered once per resize, the
// charge fill and the percentage readout are drawn per frame. Fill colour
// turns amber below 30% and red below 15%.
class BatteryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BatteryWidget(QWidget *parent = nullptr);

    void setLabel(const QString &label);
    void setValue(double percent);

    double value() const { return m_value; }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuildCache();
    QColor fillColor() const;

    double m_value = 0.0;
    QString m_label;

    // Cached static layer and geometry (rebuilt on resize/setLabel).
    QPixmap m_staticLayer;
    QPainterPath m_innerClip;
    QRectF m_innerRect;
    QRectF m_bodyRect;
    QFont m_valueFont;
};

} // namespace sc
