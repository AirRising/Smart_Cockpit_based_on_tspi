#pragma once

#include <QByteArray>
#include <QObject>
#include <QTimer>

#include "CanTypes.h"
#include "CanFrameParser.h"
#include "SystemState.h"

namespace sc {

class CanManager;

// Central vehicle data service.
//
// Lives on the GUI thread. CanManager delivers raw frames from its receive
// thread through the queued `frameReceived` -> `onCanFrame` connection, so
// all state here is only ever mutated on the main thread.
class CarService : public QObject
{
    Q_OBJECT
public:
    static constexpr int kClimateAckTimeoutMs = 600;

    explicit CarService(QObject *parent = nullptr);
    ~CarService() override;

    bool start(const QString &canInterface);
    void stop();

    bool canConnected() const;
    quint64 framesReceived() const;
    qint64 lastFrameTimeMs() const;
    CanFrameParser *parser();

    // Instrument state (0x100).
    double speed() const;
    int rpm() const;
    double fuel() const;
    bool leftIndicator() const;
    bool rightIndicator() const;
    bool hazard() const;
    bool milEngine() const;
    bool milAbs() const;
    bool milAirbag() const;

    // Gear / chassis (0x300 / 0x400).
    int gearPosition() const; // 0=P 1=R 2=N 3=D
    bool reverseEngaged() const;
    double steeringAngle() const; // deg, -900..900

    // Climate (0x200). `climate()` is the last *vehicle-confirmed* state.
    sc::ClimateState climate() const;
    bool hasPendingClimate() const;

public slots:
    // Public so unit tests / diagnostics can inject raw frames without a bus.
    void onCanFrame(quint32 id, const QByteArray &data, bool extended = false);

    // Send a climate command and wait for the vehicle echo + ack.
    // Returns a request id; on success `climateAckOk(requestId)` is emitted,
    // on timeout `climateAckTimeout(requestId)` (UI must roll back).
    quint32 sendClimateCommand(const sc::ClimateState &target);

signals:
    void speedChanged(double kmh);
    void rpmChanged(int rpm);
    void fuelChanged(double percent);
    void indicatorsChanged(bool left, bool right, bool hazard);
    void milsChanged(bool engine, bool abs, bool airbag);
    void gearChanged(int position);
    void reverseChanged(bool active);
    void steeringAngleChanged(double degrees);

    void climateStateChanged(const sc::ClimateState &state);
    void climateAckOk(quint32 requestId);
    void climateAckTimeout(quint32 requestId);

    void canConnectionChanged(bool connected);

private:
    void handleFrame100(const QByteArray &data);
    void handleFrame200(const QByteArray &data);
    void handleFrame300(const QByteArray &data);
    void handleFrame400(const QByteArray &data);

    CanFrameParser m_parser;
    CanManager *m_can = nullptr;

    qint64 m_lastFrameTimeMs = 0;
    bool m_canConnected = false;

    // 0x100
    double m_speed = 0.0;
    int m_rpm = 0;
    double m_fuel = 0.0;
    bool m_leftIndicator = false;
    bool m_rightIndicator = false;
    bool m_hazard = false;
    bool m_milEngine = false;
    bool m_milAbs = false;
    bool m_milAirbag = false;

    // 0x300 / 0x400
    int m_gearPosition = 0; // 0=P 1=R 2=N 3=D
    bool m_reverse = false;
    double m_steeringAngle = 0.0;

    // 0x200
    sc::ClimateState m_climate;
    quint32 m_climateRequestSeq = 0;
    bool m_pendingClimate = false;
    QTimer m_ackTimer;
};

} // namespace sc
