#include "VideoWidget.h"

#include <QPainter>
#include <QPainterPath>

namespace sc {

VideoWidget::VideoWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setAutoFillBackground(false);
}

void VideoWidget::setFrame(const QImage &frame)
{
    {
        QMutexLocker locker(&m_frameMutex);
        m_frame = frame;
    }
    update();
}

void VideoWidget::setSteeringAngle(double degrees)
{
    m_steeringAngle = degrees;
    update();
}

void VideoWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.02f, 0.02f, 0.02f, 1.0f);
}

void VideoWidget::resizeGL(int w, int h)
{
    Q_UNUSED(w) Q_UNUSED(h)
}

void VideoWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QImage frame;
    {
        QMutexLocker locker(&m_frameMutex);
        frame = m_frame;
    }

    if (!frame.isNull()) {
        // Keep aspect ratio, fill as much of the screen as possible.
        const QRectF target = QRectF(QPointF(0, 0), QSizeF(width(), height()));
        const QRectF fitted = target & QRectF(
            target.topLeft(),
            QSizeF(target.width(), target.width() * frame.height() / qMax(1, frame.width())));
        painter.drawImage(fitted, frame, QRectF(frame.rect()));
    } else {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, tr("NO SIGNAL"));
    }

    drawTrajectory(painter);
}

void VideoWidget::drawTrajectory(QPainter &painter)
{
    const int w = width();
    const int h = height();
    const qreal steerNorm = qBound(-1.0, m_steeringAngle / 450.0, 1.0);
    const qreal baseSpread = w * 0.10;
    const qreal bottomY = h - 30.0;
    const qreal topY = h * 0.32;
    const qreal shift = steerNorm * w * 0.18;

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(0, 255, 120, 220), 3));

    for (int side : {-1, 1}) {
        const qreal startX = w / 2.0 + side * baseSpread + shift * 0.4;
        const qreal endX = w / 2.0 + side * baseSpread * 1.8 + shift;
        const qreal bulge = w * 0.11 * (1.0 + 0.5 * qAbs(steerNorm));

        QPainterPath path;
        path.moveTo(startX, bottomY);
        path.cubicTo(startX + side * bulge, h * 0.72,
                     endX - side * bulge, h * 0.50,
                     endX, topY);
        painter.drawPath(path);
    }
}

} // namespace sc
