#pragma once

#include <QWidget>

namespace sc {

class CameraService;
class CarService;
class VideoWidget;

// Full-screen reverse-view page. Just hosts the video surface (OpenGL where
// available, software fallback elsewhere) and forwards camera frames /
// steering angle to it. ScreenManager enforces the Urgent preemption.
class ReverseCameraPage : public QWidget
{
    Q_OBJECT
public:
    explicit ReverseCameraPage(CameraService *camera, CarService *car,
                               QWidget *parent = nullptr);

private:
    VideoWidget *m_video = nullptr;
};

} // namespace sc
