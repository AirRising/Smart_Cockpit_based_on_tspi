#include "VideoWidget.h"

#include <QPainter>

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
}

} // namespace sc
