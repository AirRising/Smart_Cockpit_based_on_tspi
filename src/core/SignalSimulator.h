#pragma once

#include <QObject>
#include <QTimer>

namespace sc {

class CarService;

// Feeds synthetic CAN frames through CarService::onCanFrame so the HMI can be
// developed and demonstrated without any CAN bus or kernel CAN support (the
// RK3566 board image shipped without vcan/slcan). Emulates a short driving
// cycle: accelerate -> cruise (with indicators) -> stop -> reverse -> stop.
class SignalSimulator : public QObject
{
    Q_OBJECT
public:
    explicit SignalSimulator(CarService *car, QObject *parent = nullptr);

    void start();
    void stop();

private:
    void tick();
    void sendFrame100(int elapsedMs);
    void sendFrame300(int elapsedMs);
    void sendFrame400(int elapsedMs);

    static int reverseWindowStartMs();

    CarService *m_car = nullptr;
    QTimer m_timer;
    int m_elapsedMs = 0;
};

} // namespace sc
