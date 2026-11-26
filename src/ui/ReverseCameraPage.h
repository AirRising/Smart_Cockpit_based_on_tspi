#pragma once

#include <QWidget>

namespace sc {

class CameraService;
class VideoWidget;

// Full-screen reverse-view page. Just hosts the OpenGL video widget and
// forwards camera frames to it. ScreenManager enforces the Urgent preemption.
class ReverseCameraPage : public QWidget
{
    Q_OBJECT
public:
    explicit ReverseCameraPage(CameraService *camera, QWidget *parent = nullptr);

private:
    VideoWidget *m_video = nullptr;
};

} // namespace sc
