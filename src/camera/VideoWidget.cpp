#include "VideoWidget.h"

#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QMutex>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

namespace sc {

// Plain interface so the host VideoWidget can drive either painting backend
// polymorphically. It deliberately does not derive from QWidget: the GL
// backend must inherit QOpenGLWidget, so a QWidget-based base would yield two
// QWidget sub-objects.
class VideoSurface
{
public:
    virtual ~VideoSurface() = default;
    virtual QWidget *widget() = 0;
    virtual void setFrame(const QImage &frame) = 0;
    virtual void setSteeringAngle(double degrees) = 0;
};

namespace {

// Common frame + trajectory painting, used verbatim by both backends so the
// software path looks identical to the OpenGL one.
void drawVideoContent(QPainter &painter, const QSize &viewSize, const QImage &frame,
                      double steeringAngle)
{
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (!frame.isNull()) {
        // Keep aspect ratio, fill as much of the screen as possible.
        const QRectF target(0, 0, viewSize.width(), viewSize.height());
        const QRectF fitted = target & QRectF(
            target.topLeft(),
            QSizeF(target.width(),
                   target.width() * frame.height() / qMax(1, frame.width())));
        painter.drawImage(fitted, frame, QRectF(frame.rect()));
    } else {
        painter.setPen(Qt::white);
        painter.drawText(QRectF(QPointF(0, 0), QSizeF(viewSize)),
                         Qt::AlignCenter, QStringLiteral("NO SIGNAL"));
    }

    const int w = viewSize.width();
    const int h = viewSize.height();
    const qreal steerNorm = qBound(-1.0, steeringAngle / 450.0, 1.0);
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

// OpenGL-accelerated backend. Borrows QOpenGLWidget only as a fast texture
// upload target for the QImage; no shaders are used and the trajectory is
// painted with QPainter on top of the GL surface.
class GlVideoSurface : public QOpenGLWidget, public VideoSurface, protected QOpenGLFunctions
{
public:
    using QOpenGLWidget::QOpenGLWidget;

    QWidget *widget() override
    {
        return this;
    }

    void setFrame(const QImage &frame) override
    {
        {
            QMutexLocker locker(&m_frameMutex);
            m_frame = frame;
        }
        update();
    }

    void setSteeringAngle(double degrees) override
    {
        m_steeringAngle = degrees;
        update();
    }

protected:
    void initializeGL() override
    {
        initializeOpenGLFunctions();
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    }

    void paintGL() override
    {
        glClear(GL_COLOR_BUFFER_BIT);

        QImage frame;
        {
            QMutexLocker locker(&m_frameMutex);
            frame = m_frame;
        }

        QPainter painter(this);
        drawVideoContent(painter, size(), frame, m_steeringAngle);
    }

private:
    QImage m_frame;
    QMutex m_frameMutex;
    double m_steeringAngle = 0.0;
};

// Software backend for platforms without GL (offscreen/minimal, headless CI).
class SoftVideoSurface : public QWidget, public VideoSurface
{
public:
    using QWidget::QWidget;

    QWidget *widget() override
    {
        return this;
    }

    void setFrame(const QImage &frame) override
    {
        {
            QMutexLocker locker(&m_frameMutex);
            m_frame = frame;
        }
        update();
    }

    void setSteeringAngle(double degrees) override
    {
        m_steeringAngle = degrees;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(0x05, 0x05, 0x05));
        drawVideoContent(painter, size(), frame(), m_steeringAngle);
    }

private:
    QImage frame()
    {
        QMutexLocker locker(&m_frameMutex);
        return m_frame;
    }

    QImage m_frame;
    QMutex m_frameMutex;
    double m_steeringAngle = 0.0;
};

} // namespace

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    if (supportsOpenGL()) {
        m_view = new GlVideoSurface(this);
    } else {
        m_view = new SoftVideoSurface(this);
        qInfo() << "No OpenGL context available; reverse-camera surface uses"
                << "the software renderer";
    }
    layout->addWidget(m_view->widget());
}

bool VideoWidget::supportsOpenGL()
{
    // Allow forcing a backend from the environment (debugging/CI).
    const QByteArray backend = qgetenv("SMART_COCKPIT_VIDEO_BACKEND");
    if (backend.compare("opengl", Qt::CaseInsensitive) == 0)
        return true;
    if (backend.compare("software", Qt::CaseInsensitive) == 0)
        return false;

    // The offscreen/minimal platform plugins never provide a GL-capable
    // widget surface.
    const QString platform = QGuiApplication::platformName();
    if (platform == QLatin1String("offscreen") || platform == QLatin1String("minimal"))
        return false;

    // Probe whether a real GL context can be created on this platform.
    QOpenGLContext probe;
    return probe.create();
}

void VideoWidget::setFrame(const QImage &frame)
{
    if (m_view)
        m_view->setFrame(frame);
}

void VideoWidget::setSteeringAngle(double degrees)
{
    if (m_view)
        m_view->setSteeringAngle(degrees);
}

} // namespace sc
