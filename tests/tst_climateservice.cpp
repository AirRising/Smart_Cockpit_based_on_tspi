#include <QtTest>

#include <QPushButton>
#include <QSlider>

#include "CanFrameParser.h"
#include "CarService.h"
#include "ClimatePage.h"

using namespace sc;

namespace {

QByteArray climateFrame(CarService &service, const ClimateState &state, bool ack = true)
{
    const QHash<QString, double> values = {
        {QStringLiteral("temp_set"), state.temperature},
        {QStringLiteral("fan_speed"), state.fanSpeed},
        {QStringLiteral("blow_mode"), state.blowMode},
        {QStringLiteral("ac_on"), state.acOn ? 1.0 : 0.0},
        {QStringLiteral("auto_mode"), state.autoMode ? 1.0 : 0.0},
        {QStringLiteral("ack"), ack ? 1.0 : 0.0}
    };
    QByteArray payload;
    if (!service.parser()->encode(FrameId::HVAC, values, &payload))
        return {};
    return payload;
}

} // namespace

class TestClimateService : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void ackConfirmsCommand();
    void ackRequiresFullTarget_data();
    void ackRequiresFullTarget();
    void cancelThenNewCommandIgnoresOldAck();
    void cancelRequiresMatchingIdAndStopsTimeout();
    void lateAckIgnoredButPanelStateSyncs();
    void reentrantSendDoesNotReplacePending();
    void stopClearsPending();
    void timeoutRollsBack();
    void partialDbcKeepsLastKnownGood();
    void cancelButtonFollowsPendingState();
    void debounceCoalescesRapidChanges();
};

void TestClimateService::initTestCase()
{
    qRegisterMetaType<sc::ClimateState>("sc::ClimateState");
}

void TestClimateService::ackConfirmsCommand()
{
    CarService service;
    QSignalSpy ackSpy(&service, &CarService::climateAckOk);
    QSignalSpy stateSpy(&service, &CarService::climateStateChanged);

    ClimateState target;
    target.temperature = 24;
    target.fanSpeed = 5;
    target.blowMode = 1;
    target.acOn = true;
    target.autoMode = false;

    const quint32 requestId = service.sendClimateCommand(target);
    QVERIFY(requestId > 0);
    QVERIFY(service.hasPendingClimate());

    // Vehicle echo with ack bit set.
    QHash<QString, double> echo;
    echo.insert(QStringLiteral("temp_set"), 24.0);
    echo.insert(QStringLiteral("fan_speed"), 5.0);
    echo.insert(QStringLiteral("blow_mode"), 1.0);
    echo.insert(QStringLiteral("ac_on"), 1.0);
    echo.insert(QStringLiteral("auto_mode"), 0.0);
    echo.insert(QStringLiteral("ack"), 1.0);
    QByteArray payload;
    QVERIFY(service.parser()->encode(FrameId::HVAC, echo, &payload));
    service.onCanFrame(FrameId::HVAC, payload);

    // onCanFrame is invoked synchronously here, so the ack is already captured
    // (Qt 5.15's wait() only returns true for emissions that happen while
    // waiting; see QTBUG-85030).
    QVERIFY(ackSpy.count() == 1);
    QCOMPARE(ackSpy.at(0).at(0).toUInt(), requestId);
    QVERIFY(!service.hasPendingClimate());
    QCOMPARE(service.climate().temperature, 24);
    QCOMPARE(service.climate().fanSpeed, 5);
    QCOMPARE(stateSpy.count(), 1);
}

void TestClimateService::ackRequiresFullTarget_data()
{
    QTest::addColumn<ClimateState>("echoed");
    ClimateState echoed;
    echoed.temperature = 24;
    QTest::newRow("temperature") << echoed;
    echoed = ClimateState{};
    echoed.fanSpeed = 5;
    QTest::newRow("fan") << echoed;
    echoed = ClimateState{};
    echoed.blowMode = 1;
    QTest::newRow("blow-mode") << echoed;
    echoed = ClimateState{};
    echoed.acOn = false;
    QTest::newRow("ac") << echoed;
    echoed = ClimateState{};
    echoed.autoMode = true;
    QTest::newRow("auto") << echoed;
}

void TestClimateService::ackRequiresFullTarget()
{
    QFETCH(ClimateState, echoed);
    CarService service;
    QSignalSpy ackSpy(&service, &CarService::climateAckOk);
    QSignalSpy stateSpy(&service, &CarService::climateStateChanged);
    const ClimateState target;
    const quint32 requestId = service.sendClimateCommand(target);
    QVERIFY(requestId != 0);

    const QByteArray mismatched = climateFrame(service, echoed);
    QVERIFY(!mismatched.isEmpty());
    service.onCanFrame(FrameId::HVAC, mismatched);
    QVERIFY(service.hasPendingClimate());
    QCOMPARE(ackSpy.count(), 0);
    QCOMPARE(stateSpy.count(), 0);
    QVERIFY(service.climate() == target);

    const QByteArray matching = climateFrame(service, target);
    QVERIFY(!matching.isEmpty());
    // An incomplete echo must not complete the request either.
    service.onCanFrame(FrameId::HVAC, matching.left(2));
    QVERIFY(service.hasPendingClimate());
    QCOMPARE(ackSpy.count(), 0);

    service.onCanFrame(FrameId::HVAC, matching);
    QVERIFY(!service.hasPendingClimate());
    QCOMPARE(ackSpy.count(), 1);
    QCOMPARE(ackSpy.at(0).at(0).toUInt(), requestId);
}

void TestClimateService::cancelThenNewCommandIgnoresOldAck()
{
    CarService service;
    QSignalSpy ackSpy(&service, &CarService::climateAckOk);
    QSignalSpy stateSpy(&service, &CarService::climateStateChanged);
    ClimateState target;
    target.temperature = 24;
    const quint32 oldId = service.sendClimateCommand(target);
    QVERIFY(oldId != 0);
    const QByteArray oldAck = climateFrame(service, target);
    QVERIFY(!oldAck.isEmpty());
    QVERIFY(service.cancelClimateCommand(oldId));
    QVERIFY(!service.hasPendingClimate());

    target.temperature = 28;
    const quint32 newId = service.sendClimateCommand(target);
    QVERIFY(newId != 0 && newId != oldId);
    QVERIFY(!service.cancelClimateCommand(oldId));
    service.onCanFrame(FrameId::HVAC, oldAck);
    QVERIFY(service.hasPendingClimate());
    QCOMPARE(ackSpy.count(), 0);
    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(service.climate().temperature, 22);

    const QByteArray newAck = climateFrame(service, target);
    QVERIFY(!newAck.isEmpty());
    service.onCanFrame(FrameId::HVAC, newAck);
    QVERIFY(!service.hasPendingClimate());
    QCOMPARE(ackSpy.count(), 1);
    QCOMPARE(ackSpy.at(0).at(0).toUInt(), newId);
    QCOMPARE(stateSpy.count(), 1);
    QVERIFY(service.climate() == target);
}

void TestClimateService::cancelRequiresMatchingIdAndStopsTimeout()
{
    CarService service;
    QSignalSpy cancelledSpy(&service, &CarService::climateCommandCancelled);
    QSignalSpy timeoutSpy(&service, &CarService::climateAckTimeout);
    QSignalSpy ackSpy(&service, &CarService::climateAckOk);
    const ClimateState confirmed = service.climate();
    ClimateState target = confirmed;
    target.temperature = 24;
    const quint32 requestId = service.sendClimateCommand(target);
    QVERIFY(requestId != 0);
    QVERIFY(!service.cancelClimateCommand(0));
    QVERIFY(!service.cancelClimateCommand(requestId + 1));
    QVERIFY(service.hasPendingClimate());
    QCOMPARE(cancelledSpy.count(), 0);

    QVERIFY(service.cancelClimateCommand(requestId));
    QVERIFY(!service.hasPendingClimate());
    QCOMPARE(cancelledSpy.count(), 1);
    QCOMPARE(cancelledSpy.at(0).at(0).toUInt(), requestId);
    QVERIFY(!service.cancelClimateCommand(requestId));
    QTest::qWait(CarService::kClimateAckTimeoutMs + 150);
    QCOMPARE(cancelledSpy.count(), 1);
    QCOMPARE(timeoutSpy.count(), 0);
    QCOMPARE(ackSpy.count(), 0);
    QVERIFY(service.climate() == confirmed);
}

void TestClimateService::lateAckIgnoredButPanelStateSyncs()
{
    CarService service;
    QSignalSpy ackSpy(&service, &CarService::climateAckOk);
    QSignalSpy stateSpy(&service, &CarService::climateStateChanged);
    const ClimateState confirmed = service.climate();
    ClimateState target = confirmed;
    target.temperature = 28;
    target.fanSpeed = 5;
    const quint32 requestId = service.sendClimateCommand(target);
    QVERIFY(requestId != 0);
    QVERIFY(service.cancelClimateCommand(requestId));

    const QByteArray lateAck = climateFrame(service, target);
    QVERIFY(!lateAck.isEmpty());
    service.onCanFrame(FrameId::HVAC, lateAck);
    QVERIFY(!service.hasPendingClimate());
    QVERIFY(service.climate() == confirmed);
    QCOMPARE(ackSpy.count(), 0);
    QCOMPARE(stateSpy.count(), 0);

    const QByteArray panelState = climateFrame(service, target, false);
    QVERIFY(!panelState.isEmpty());
    service.onCanFrame(FrameId::HVAC, panelState);
    QVERIFY(service.climate() == target);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(ackSpy.count(), 0);
    service.onCanFrame(FrameId::HVAC, panelState);
    QCOMPARE(stateSpy.count(), 1);
}

void TestClimateService::reentrantSendDoesNotReplacePending()
{
    CarService service;
    QSignalSpy ackSpy(&service, &CarService::climateAckOk);
    ClimateState target;
    target.temperature = 24;
    const quint32 requestId = service.sendClimateCommand(target);
    QVERIFY(requestId != 0);
    const QByteArray originalAck = climateFrame(service, target);
    QVERIFY(!originalAck.isEmpty());

    target.temperature = 28;
    QCOMPARE(service.sendClimateCommand(target), quint32(0));
    QVERIFY(service.hasPendingClimate());
    const QByteArray rejectedAck = climateFrame(service, target);
    QVERIFY(!rejectedAck.isEmpty());
    service.onCanFrame(FrameId::HVAC, rejectedAck);
    QVERIFY(service.hasPendingClimate());
    QCOMPARE(ackSpy.count(), 0);

    service.onCanFrame(FrameId::HVAC, originalAck);
    QVERIFY(!service.hasPendingClimate());
    QCOMPARE(ackSpy.count(), 1);
    QCOMPARE(ackSpy.at(0).at(0).toUInt(), requestId);
    QCOMPARE(service.climate().temperature, 24);
}

void TestClimateService::stopClearsPending()
{
    CarService service;
    QSignalSpy cancelledSpy(&service, &CarService::climateCommandCancelled);
    QSignalSpy timeoutSpy(&service, &CarService::climateAckTimeout);
    ClimateState target;
    target.temperature = 24;
    const quint32 requestId = service.sendClimateCommand(target);
    QVERIFY(requestId != 0);
    service.stop();
    QVERIFY(!service.hasPendingClimate());
    QCOMPARE(cancelledSpy.count(), 1);
    QCOMPARE(cancelledSpy.at(0).at(0).toUInt(), requestId);
    service.stop();
    QCOMPARE(cancelledSpy.count(), 1);
    QTest::qWait(CarService::kClimateAckTimeoutMs + 150);
    QCOMPARE(timeoutSpy.count(), 0);

    const quint32 newId = service.sendClimateCommand(target);
    QVERIFY(newId != 0 && newId != requestId);
    QVERIFY(service.cancelClimateCommand(newId));
}

void TestClimateService::timeoutRollsBack()
{
    CarService service;
    QSignalSpy timeoutSpy(&service, &CarService::climateAckTimeout);
    QSignalSpy ackSpy(&service, &CarService::climateAckOk);
    QSignalSpy stateSpy(&service, &CarService::climateStateChanged);

    ClimateState target;
    target.temperature = 28;
    const quint32 requestId = service.sendClimateCommand(target);
    QVERIFY(requestId != 0);
    ClimateState oldTarget = target;
    oldTarget.temperature = 24;
    const QByteArray oldAck = climateFrame(service, oldTarget);
    QVERIFY(!oldAck.isEmpty());
    service.onCanFrame(FrameId::HVAC, oldAck);
    QVERIFY(service.hasPendingClimate());
    QCOMPARE(ackSpy.count(), 0);

    QVERIFY(timeoutSpy.wait(CarService::kClimateAckTimeoutMs + 800));
    QCOMPARE(timeoutSpy.count(), 1);
    QCOMPARE(timeoutSpy.at(0).at(0).toUInt(), requestId);
    QVERIFY(!service.hasPendingClimate());
    // A matching but late ACK after timeout must not change confirmed state.
    const QByteArray lateAck = climateFrame(service, target);
    QVERIFY(!lateAck.isEmpty());
    service.onCanFrame(FrameId::HVAC, lateAck);
    QCOMPARE(ackSpy.count(), 0);
    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(service.climate().temperature, 22);
    QCOMPARE(service.climate().fanSpeed, 3);
}

void TestClimateService::partialDbcKeepsLastKnownGood()
{
    CarService service;
    QSignalSpy speedSpy(&service, &CarService::speedChanged);

    // Normal frame: all signals present -> speed updates to 120.
    service.onCanFrame(FrameId::ICU_Dynamic, QByteArray::fromHex("7800000000000000"));
    QCOMPARE(service.speed(), 120.0);
    QCOMPARE(speedSpy.count(), 1);

    // Now override 0x100 so it lacks most signals (simulating a real DBC whose
    // field names differ). The next frame must NOT clobber the last value.
    const QString dbc = QStringLiteral(
        "BO_ 256 ICU: 8 ICU\n"
        " SG_ speed : 0|16@1+ (1,0) [0|240] \"km/h\" ICU\n");
    QVERIFY(service.parser()->loadDbcText(dbc));

    service.onCanFrame(FrameId::ICU_Dynamic, QByteArray::fromHex("c800000000000000")); // 200 km/h
    // Kept the last known-good speed instead of showing partial data.
    QCOMPARE(service.speed(), 120.0);
    QCOMPARE(speedSpy.count(), 1);
}

// The cancel-pending button must only be clickable while a climate command is
// waiting for the vehicle ack: enabled when the UI issues a command, disabled
// again once the ack arrives.
void TestClimateService::cancelButtonFollowsPendingState()
{
    CarService car;
    ClimatePage page(&car);
    page.show();
    QCoreApplication::processEvents();

    auto *cancel = page.findChild<QPushButton *>(QStringLiteral("cancelPending"));
    QVERIFY(cancel);
    QVERIFY(!cancel->isEnabled());

    QSlider *temp = nullptr;
    const auto sliders = page.findChildren<QSlider *>();
    for (QSlider *s : sliders) {
        if (s->minimum() == 16) {
            temp = s;
            break;
        }
    }
    QVERIFY(temp);

    // User changes the set point -> after the debounce a command is issued
    // and cancel becomes enabled.
    temp->setValue(24);
    QTRY_VERIFY(car.hasPendingClimate());
    QVERIFY(cancel->isEnabled());

    // Vehicle echoes with the ack bit set -> pending cleared -> disabled again.
    QHash<QString, double> echo;
    echo.insert(QStringLiteral("temp_set"), 24.0);
    echo.insert(QStringLiteral("fan_speed"), 3.0);
    echo.insert(QStringLiteral("blow_mode"), 0.0);
    echo.insert(QStringLiteral("ac_on"), 1.0);
    echo.insert(QStringLiteral("auto_mode"), 0.0);
    echo.insert(QStringLiteral("ack"), 1.0);
    QByteArray payload;
    QVERIFY(car.parser()->encode(FrameId::HVAC, echo, &payload));
    car.onCanFrame(FrameId::HVAC, payload);
    QCoreApplication::processEvents();

    QVERIFY(!car.hasPendingClimate());
    QVERIFY(!cancel->isEnabled());
}

// Rapid successive control changes must be coalesced by the debounce timer:
// nothing is sent while the user is still adjusting, and exactly one command
// (observable via its single ack timeout) goes out afterwards.
void TestClimateService::debounceCoalescesRapidChanges()
{
    CarService car;
    ClimatePage page(&car);
    page.show();
    QCoreApplication::processEvents();

    QSlider *temp = nullptr;
    const auto sliders = page.findChildren<QSlider *>();
    for (QSlider *s : sliders) {
        if (s->minimum() == 16) {
            temp = s;
            break;
        }
    }
    QVERIFY(temp);

    QSignalSpy timeoutSpy(&car, &CarService::climateAckTimeout);

    // Four quick changes inside the debounce window: no command sent yet.
    temp->setValue(17);
    temp->setValue(18);
    temp->setValue(19);
    temp->setValue(20);
    QVERIFY(!car.hasPendingClimate());

    // The debounce expires and exactly one command is issued.
    QTRY_VERIFY(car.hasPendingClimate());

    // No vehicle echo -> exactly one ack timeout proves exactly one command.
    QVERIFY(timeoutSpy.wait(CarService::kClimateAckTimeoutMs + 800));
    QCOMPARE(timeoutSpy.count(), 1);
}

QTEST_MAIN(TestClimateService)
#include "tst_climateservice.moc"
