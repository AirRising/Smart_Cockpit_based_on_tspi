#pragma once

#include <QImage>
#include <QWidget>

namespace sc {

class VideoSurface;

// Reverse-camera surface host.
//
// The real drawing is delegated to one of two backends selected at runtime:
//  - an OpenGL-accelerated surface (QOpenGLWidget), used whenever the current
//    Qt platform can provide a GL context (eglfs/KMS on the RK3566, a desktop
//    with a GPU, ...);
//  - a software QPainter fallback for platforms without GL. Exposing a
//    QOpenGLWidget there is fatal in Qt6 (the backing-store RHI flush crashes
//    e.g. under the "offscreen" plugin), so headless/CI runs stay healthy.
class VideoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);

    // True when the current platform can provide a GL-capable widget surface.
    static bool supportsOpenGL();

public slots:
    void setFrame(const QImage &frame);
    void setSteeringAngle(double degrees);

private:
    VideoSurface *m_view = nullptr;
};

} // namespace sc
