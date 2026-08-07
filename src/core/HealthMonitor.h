#pragma once

#include <QObject>
#include <QTimer>

#include "SystemState.h"

namespace sc {

class CarService;
class CameraService;
class MediaService;

// Periodically polls service health and drives the system state machine:
//   - CAN frame flow stopped     -> Emergency
//   - camera / media degraded    -> Degraded
//   - everything healthy         -> Normal
class HealthMonitor : public QObject
{
    Q_OBJECT
public:
    HealthMonitor(CarService *car, CameraService *camera, MediaService *media,
                  QObject *parent = nullptr);

    void start(int intervalMs = 2000);
    void stop();

    sc::SystemState state() const;

signals:
    void stateChanged(sc::SystemState state);
    void diagnosticsChanged(const QString &summary);

private slots:
    void poll();

private:
    CarService *m_car = nullptr;
    CameraService *m_camera = nullptr;
    MediaService *m_media = nullptr;

    QTimer m_timer;
    sc::SystemState m_state = sc::SystemState::Normal;
};

} // namespace sc
