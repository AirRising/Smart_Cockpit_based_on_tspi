#include "HealthMonitor.h"

#include <QDateTime>

#include "CameraService.h"
#include "CarService.h"
#include "MediaService.h"

namespace sc {

HealthMonitor::HealthMonitor(CarService *car, CameraService *camera, MediaService *media,
                             QObject *parent)
    : QObject(parent)
    , m_car(car)
    , m_camera(camera)
    , m_media(media)
{
    m_timer.setInterval(2000);
    connect(&m_timer, &QTimer::timeout, this, &HealthMonitor::poll);
}

void HealthMonitor::start(int intervalMs)
{
    m_timer.setInterval(intervalMs);
    m_timer.start();
    poll();
}

void HealthMonitor::stop()
{
    m_timer.stop();
}

sc::SystemState HealthMonitor::state() const
{
    return m_state;
}

void HealthMonitor::poll()
{
    const quint64 framesNow = m_car ? m_car->framesReceived() : 0;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool canAlive = m_car && m_car->canConnected() &&
                          now - m_car->lastFrameTimeMs() < 3000;

    const bool cameraAlive = m_camera && m_camera->isRunning() &&
                             m_camera->lastFrameAgeMs() < 2500;
    const bool mediaAlive = m_media && m_media->healthy();

    sc::SystemState next = sc::SystemState::Normal;
    if (!canAlive)
        next = sc::SystemState::Emergency;
    else if (!cameraAlive || !mediaAlive)
        next = sc::SystemState::Degraded;

    if (next != m_state) {
        m_state = next;
        emit stateChanged(m_state);
    }

    emit diagnosticsChanged(QStringLiteral("CAN:%1 CAM:%2 MEDIA:%3 frames:%4")
                                .arg(canAlive ? QStringLiteral("OK") : QStringLiteral("LOST"),
                                     cameraAlive ? QStringLiteral("OK") : QStringLiteral("FAIL"),
                                     mediaAlive ? QStringLiteral("OK") : QStringLiteral("FAIL"))
                                .arg(framesNow));
}

} // namespace sc
