#pragma once

#include <QImage>
#include <QMutex>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>

namespace sc {

// OpenGL-accelerated reverse-view widget.
//
// The latest camera frame is uploaded/drawn every paint; the dynamic reverse
// trajectory (two guide curves derived from the steering angle) is overlaid
// with QPainter on top of the GL surface.
class VideoWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);

public slots:
    void setFrame(const QImage &frame);
    void setSteeringAngle(double degrees);
    void setOverlayVisible(bool visible);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    void drawTrajectory(QPainter &painter);

    QImage m_frame;
    QMutex m_frameMutex;
    double m_steeringAngle = 0.0;
    bool m_overlayVisible = true;
};

} // namespace sc
