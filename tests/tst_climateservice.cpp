#include <QtTest>

#include "CanFrameParser.h"
#include "CarService.h"

using namespace sc;

class TestClimateService : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void ackConfirmsCommand();
    void timeoutRollsBack();
    void partialDbcKeepsLastKnownGood();
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

void TestClimateService::timeoutRollsBack()
{
    CarService service;
    QSignalSpy timeoutSpy(&service, &CarService::climateAckTimeout);

    ClimateState target;
    target.temperature = 28;
    service.sendClimateCommand(target);

    QVERIFY(timeoutSpy.wait(CarService::kClimateAckTimeoutMs + 800));
    QCOMPARE(timeoutSpy.count(), 1);
    QVERIFY(!service.hasPendingClimate());
    // Rolled back: confirmed state still has the default values.
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

QTEST_MAIN(TestClimateService)
#include "tst_climateservice.moc"
