#include "SignalSimulator.h"

#include "CanTypes.h"
#include "CanFrameParser.h"
#include "CarService.h"

#include <QtMath>

namespace sc {

namespace {

// A complete "lap" of the demo cycle, in milliseconds.
constexpr int kLapMs = 60000;
constexpr int kTickMs = 100;

// Segment boundaries inside one lap.
constexpr int kAccelEnd = 8000;    // 0..8s   : accelerate 0 -> 90
constexpr int kCruiseEnd = 30000;  // 8..30s  : cruise at ~90, indicators on
constexpr int kStopStart = 40000;  // 30..40s : brake to a stop
constexpr int kReverseStart = 45000; // 45..52s: reverse engaged (shows camera)
constexpr int kReverseEnd = 52000;

int lapMs(int elapsedMs) { return elapsedMs % kLapMs; }

} // namespace

int SignalSimulator::reverseWindowStartMs()
{
    return kReverseStart;
}

SignalSimulator::SignalSimulator(CarService *car, QObject *parent)
    : QObject(parent)
    , m_car(car)
{
    m_timer.setInterval(kTickMs);
    connect(&m_timer, &QTimer::timeout, this, &SignalSimulator::tick);
}

void SignalSimulator::start()
{
    m_elapsedMs = 0;
    m_timer.start();
}

void SignalSimulator::stop()
{
    m_timer.stop();
}

void SignalSimulator::tick()
{
    m_elapsedMs += kTickMs;
    sendFrame100(m_elapsedMs);
    sendFrame300(m_elapsedMs);
    sendFrame400(m_elapsedMs);
}

void SignalSimulator::sendFrame100(int elapsedMs)
{
    const int p = lapMs(elapsedMs);
    double speed = 0.0;
    if (p < kAccelEnd) {
        speed = 90.0 * p / kAccelEnd;
    } else if (p < kCruiseEnd) {
        speed = 90.0;
    } else if (p < kStopStart) {
        speed = 90.0 * (1.0 - double(p - kCruiseEnd) / (kStopStart - kCruiseEnd));
    }
    // Smooth the value so the needle moves continuously between ticks.
    speed = qMax(0.0, speed);

    const double rpm = 800.0 + qMax(0.0, speed) * 32.0; // ~90 km/h -> ~3700 rpm
    const double fuel = 45.0 - (p / double(kLapMs)) * 2.0;

    const bool cruising = speed >= 5.0;
    const bool left = cruising && (p / 1000) % 12 >= 6;
    const bool right = cruising && (p / 1000) % 12 >= 9;
    const bool hazard = (elapsedMs / 1000) % 25 >= 22;
    const bool milEngine = (elapsedMs / 1000) % 40 >= 37;

    QHash<QString, double> values;
    values.insert(QStringLiteral("speed"), speed);
    values.insert(QStringLiteral("engine_rpm"), rpm);
    values.insert(QStringLiteral("fuel_level"), qMax(5.0, fuel));
    values.insert(QStringLiteral("left_indicator"), left ? 1.0 : 0.0);
    values.insert(QStringLiteral("right_indicator"), right ? 1.0 : 0.0);
    values.insert(QStringLiteral("hazard"), hazard ? 1.0 : 0.0);
    values.insert(QStringLiteral("mil_engine"), milEngine ? 1.0 : 0.0);
    values.insert(QStringLiteral("mil_abs"), 0.0);
    values.insert(QStringLiteral("mil_airbag"), 0.0);

    QByteArray payload;
    if (m_car && m_car->parser()->encode(FrameId::ICU_Dynamic, values, &payload))
        m_car->onCanFrame(FrameId::ICU_Dynamic, payload);
}

void SignalSimulator::sendFrame300(int elapsedMs)
{
    const int p = lapMs(elapsedMs);
    const bool reversing = p >= kReverseStart && p < kReverseEnd;
    // gear_signal byte: bit0 = R engaged, bits 1..3 = 0=P 1=R 2=N 3=D.
    int raw = 0x00; // P
    if (reversing)
        raw = 0x03; // R + reverse bit
    else if (p < kStopStart)
        raw = 0x06; // D (accelerating / cruising / braking)

    QHash<QString, double> values;
    values.insert(QStringLiteral("gear_signal"), raw);
    QByteArray payload;
    if (m_car && m_car->parser()->encode(FrameId::Gear, values, &payload))
        m_car->onCanFrame(FrameId::Gear, payload);
}

void SignalSimulator::sendFrame400(int elapsedMs)
{
    const int p = lapMs(elapsedMs);
    const bool moving = p < kStopStart && p >= kAccelEnd;
    const double angle = moving ? 60.0 * qSin(elapsedMs / 4000.0) : 0.0;

    QHash<QString, double> values;
    values.insert(QStringLiteral("steering_angle"), angle);
    QByteArray payload;
    if (m_car && m_car->parser()->encode(FrameId::Chassis, values, &payload))
        m_car->onCanFrame(FrameId::Chassis, payload);
}

} // namespace sc
