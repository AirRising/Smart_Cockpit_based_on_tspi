#include "CarService.h"

#include <QDateTime>
#include <QDebug>
#include <QtGlobal>

#include "CanFrameParser.h"
#include "CanManager.h"

namespace sc {

namespace {

// All-or-nothing signal check. If the parser did not deliver every signal a
// handler needs (e.g. a DBC override renamed a field, or the frame layout
// changed), keep the last known-good state instead of applying partial or
// zeroed values.
bool hasAllSignals(const QHash<QString, double> &s,
                   std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        if (!s.contains(QString::fromLatin1(key)))
            return false;
    }
    return true;
}

} // namespace

CarService::CarService(QObject *parent)
    : QObject(parent)
    , m_can(new CanManager(this))
{
    connect(m_can, &CanManager::frameReceived,
            this, &CarService::onCanFrame, Qt::QueuedConnection);
    connect(m_can, &CanManager::errorOccurred, this, [](const QString &msg) {
        qWarning() << "CAN error:" << msg;
    });

    m_ackTimer.setSingleShot(true);
    m_ackTimer.setInterval(kClimateAckTimeoutMs);
    connect(&m_ackTimer, &QTimer::timeout, this, [this]() {
        if (!m_pendingClimate)
            return;
        m_pendingClimate = false;
        qWarning() << "Climate command" << m_climateRequestSeq << "timed out, rolling back to"
                   << m_climate.temperature << "C fan" << m_climate.fanSpeed;
        emit climateAckTimeout(m_climateRequestSeq);
    });
}

CarService::~CarService()
{
    stop();
}

bool CarService::start(const QString &canInterface)
{
    return m_can->open(canInterface);
}

void CarService::stop()
{
    m_ackTimer.stop();
    if (m_can)
        m_can->closeBus();
}

bool CarService::canConnected() const
{
    return m_canConnected && m_can && m_can->isOpen();
}

quint64 CarService::framesReceived() const
{
    return m_can ? m_can->framesReceived() : 0;
}

qint64 CarService::lastFrameTimeMs() const
{
    return m_lastFrameTimeMs;
}

CanFrameParser *CarService::parser()
{
    return &m_parser;
}

double CarService::speed() const { return m_speed; }
int CarService::rpm() const { return m_rpm; }
double CarService::fuel() const { return m_fuel; }
bool CarService::leftIndicator() const { return m_leftIndicator; }
bool CarService::rightIndicator() const { return m_rightIndicator; }
bool CarService::hazard() const { return m_hazard; }
bool CarService::milEngine() const { return m_milEngine; }
bool CarService::milAbs() const { return m_milAbs; }
bool CarService::milAirbag() const { return m_milAirbag; }
int CarService::gearPosition() const { return m_gearPosition; }
bool CarService::reverseEngaged() const { return m_reverse; }
double CarService::steeringAngle() const { return m_steeringAngle; }
sc::ClimateState CarService::climate() const { return m_climate; }
bool CarService::hasPendingClimate() const { return m_pendingClimate; }

void CarService::onCanFrame(quint32 id, const QByteArray &data, bool extended)
{
    Q_UNUSED(extended)

    if (id >= 0x80000000U)
        return; // skip error frames

    m_lastFrameTimeMs = QDateTime::currentMSecsSinceEpoch();
    if (!m_canConnected) {
        m_canConnected = true;
        emit canConnectionChanged(true);
    }

    switch (id) {
    case FrameId::ICU_Dynamic: handleFrame100(data); break;
    case FrameId::HVAC:        handleFrame200(data); break;
    case FrameId::Gear:        handleFrame300(data); break;
    case FrameId::Chassis:     handleFrame400(data); break;
    default:
        qDebug() << "Unhandled frame 0x" << Qt::hex << id << Qt::dec << data.toHex();
        break;
    }
}

quint32 CarService::sendClimateCommand(const sc::ClimateState &target)
{
    ++m_climateRequestSeq;
    const quint32 requestId = m_climateRequestSeq;

    QHash<QString, double> values;
    values.insert(QStringLiteral("temp_set"), target.temperature);
    values.insert(QStringLiteral("fan_speed"), target.fanSpeed);
    values.insert(QStringLiteral("blow_mode"), target.blowMode);
    values.insert(QStringLiteral("ac_on"), target.acOn ? 1.0 : 0.0);
    values.insert(QStringLiteral("auto_mode"), target.autoMode ? 1.0 : 0.0);

    QByteArray payload;
    if (!m_parser.encode(FrameId::HVAC, values, &payload)) {
        qWarning() << "Failed to encode 0x200 climate command";
        return 0;
    }
    if (m_can->isOpen()) {
        if (m_can->sendFrame(FrameId::HVAC, payload) < 0)
            qWarning() << "sendFrame(0x200) failed";
    } else {
        qWarning() << "CAN not open - climate command not sent (simulation/test mode)";
    }

    m_pendingClimate = true;
    m_ackTimer.start();
    qInfo() << "Climate command" << requestId << "sent:" << target.temperature << "C fan"
            << target.fanSpeed << "ac" << target.acOn << "auto" << target.autoMode;
    return requestId;
}

void CarService::handleFrame100(const QByteArray &data)
{
    QHash<QString, double> s;
    if (!m_parser.decode(FrameId::ICU_Dynamic, data, &s))
        return;
    if (!hasAllSignals(s, {"speed", "engine_rpm", "fuel_level", "left_indicator",
                           "right_indicator", "hazard", "mil_engine", "mil_abs",
                           "mil_airbag"}))
        return;

    const double speed = qBound(0.0, s.value(QStringLiteral("speed")), 240.0);
    const int rpm = qBound(0, qRound(s.value(QStringLiteral("engine_rpm"))), 8000);
    const double fuel = qBound(0.0, s.value(QStringLiteral("fuel_level")), 100.0);
    const bool left = s.value(QStringLiteral("left_indicator")) > 0.5;
    const bool right = s.value(QStringLiteral("right_indicator")) > 0.5;
    const bool hazard = s.value(QStringLiteral("hazard")) > 0.5;
    const bool milEngine = s.value(QStringLiteral("mil_engine")) > 0.5;
    const bool milAbs = s.value(QStringLiteral("mil_abs")) > 0.5;
    const bool milAirbag = s.value(QStringLiteral("mil_airbag")) > 0.5;

    if (!qFuzzyCompare(speed, m_speed)) { m_speed = speed; emit speedChanged(m_speed); }
    if (rpm != m_rpm) { m_rpm = rpm; emit rpmChanged(m_rpm); }
    if (!qFuzzyCompare(fuel, m_fuel)) { m_fuel = fuel; emit fuelChanged(m_fuel); }
    if (left != m_leftIndicator || right != m_rightIndicator || hazard != m_hazard) {
        m_leftIndicator = left;
        m_rightIndicator = right;
        m_hazard = hazard;
        emit indicatorsChanged(left, right, hazard);
    }
    if (milEngine != m_milEngine || milAbs != m_milAbs || milAirbag != m_milAirbag) {
        m_milEngine = milEngine;
        m_milAbs = milAbs;
        m_milAirbag = milAirbag;
        emit milsChanged(milEngine, milAbs, milAirbag);
    }
}

void CarService::handleFrame200(const QByteArray &data)
{
    QHash<QString, double> s;
    if (!m_parser.decode(FrameId::HVAC, data, &s))
        return;
    if (!hasAllSignals(s, {"temp_set", "fan_speed", "blow_mode", "ac_on", "auto_mode",
                           "ack"}))
        return;

    sc::ClimateState echoed;
    echoed.temperature = qBound(16, qRound(s.value(QStringLiteral("temp_set"))), 30);
    echoed.fanSpeed = qBound(0, qRound(s.value(QStringLiteral("fan_speed"))), 7);
    echoed.blowMode = qBound(0, qRound(s.value(QStringLiteral("blow_mode"))), 3);
    echoed.acOn = s.value(QStringLiteral("ac_on")) > 0.5;
    echoed.autoMode = s.value(QStringLiteral("auto_mode")) > 0.5;
    const bool ack = s.value(QStringLiteral("ack")) > 0.5;

    if (ack && m_pendingClimate) {
        // Vehicle confirmed our command.
        m_pendingClimate = false;
        m_ackTimer.stop();
        m_climate = echoed;
        emit climateAckOk(m_climateRequestSeq);
        emit climateStateChanged(m_climate);
        qInfo() << "Climate ack OK:" << echoed.temperature << "C fan" << echoed.fanSpeed;
    } else if (!m_pendingClimate && echoed != m_climate) {
        // Vehicle state changed on its own (physical panel), follow it.
        m_climate = echoed;
        emit climateStateChanged(m_climate);
    }
}

void CarService::handleFrame300(const QByteArray &data)
{
    if (data.isEmpty())
        return;
    const quint8 raw = static_cast<quint8>(data.at(0));
    const bool reverse = (raw & 0x01) != 0;            // bit0 = R engaged
    const int position = qBound(0, (raw >> 1) & 0x07, 3); // 0=P 1=R 2=N 3=D

    if (reverse != m_reverse) {
        m_reverse = reverse;
        emit reverseChanged(m_reverse);
        qInfo() << "Reverse" << (m_reverse ? "engaged" : "disengaged");
    }
    if (position != m_gearPosition) {
        m_gearPosition = position;
        emit gearChanged(m_gearPosition);
    }
}

void CarService::handleFrame400(const QByteArray &data)
{
    QHash<QString, double> s;
    if (!m_parser.decode(FrameId::Chassis, data, &s))
        return;
    if (!hasAllSignals(s, {"steering_angle"}))
        return;
    const double angle = qBound(-900.0, s.value(QStringLiteral("steering_angle")), 900.0);
    if (!qFuzzyCompare(angle, m_steeringAngle)) {
        m_steeringAngle = angle;
        emit steeringAngleChanged(m_steeringAngle);
    }
}

} // namespace sc
