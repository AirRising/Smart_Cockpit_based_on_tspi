#pragma once

#include <QImage>
#include <QMutex>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>

namespace sc {

// OpenGL-accelerated reverse-view widget.
//
// The latest camera frame is uploaded/drawn every paint, scaled to fill the
// panel while keeping its aspect ratio.
class VideoWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);

public slots:
    void setFrame(const QImage &frame);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    QImage m_frame;
    QMutex m_frameMutex;
};

} // namespace sc
