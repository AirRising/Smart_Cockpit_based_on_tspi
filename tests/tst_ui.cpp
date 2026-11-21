#include <QtTest>

#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

#include "BatteryWidget.h"
#include "CarService.h"
#include "ClimatePage.h"
#include "GaugeWidget.h"
#include "InstrumentPage.h"
#include "MediaPage.h"
#include "MediaService.h"
#include "SignalSimulator.h"

using namespace sc;

namespace {

// Smoke helper: render a widget offscreen and require enough visible pixels so
// that a page that fails to paint (or paints over a broken layout) is caught.
bool paintsContent(QWidget &w)
{
    w.show();
    QCoreApplication::processEvents();

    QPixmap pm(w.size());
    pm.fill(Qt::black);
    w.render(&pm);

    const QImage img = pm.toImage();
    int bright = 0;
    for (int y = 0; y < img.height(); y += 4) {
        for (int x = 0; x < img.width(); x += 4) {
            if (qGray(img.pixel(x, y)) > 60)
                ++bright;
        }
    }
    return bright > 25;
}

QSlider *findSlider(QWidget &page, int minimum)
{
    const auto sliders = page.findChildren<QSlider *>();
    for (QSlider *s : sliders) {
        if (s->minimum() == minimum)
            return s;
    }
    return nullptr;
}

} // namespace

// Headless smoke tests: construct the real pages, drive them through a real
// CarService and make sure signal wiring + painting hold together. No board or
// CAN bus required (frames are injected synchronously via onCanFrame).
class TestUiSmoke : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void instrumentGaugesFollowCanFrames();
    void climateRoundTripThroughUi();
    void climateCancelThenNewRequest();
    void climateRejectedSendDoesNotLockUi();
    void simulatorDrivesInstrument();
    void pagesRenderSomething();
};

void TestUiSmoke::initTestCase()
{
    qRegisterMetaType<sc::ClimateState>("sc::ClimateState");
}

void TestUiSmoke::instrumentGaugesFollowCanFrames()
{
    CarService car;
    InstrumentPage page(&car);
    page.show();
    QCoreApplication::processEvents();

    // 0x100: speed=120, rpm=3000, fuel=50%, left indicator.
    car.onCanFrame(FrameId::ICU_Dynamic, QByteArray::fromHex("7800b80b64010000"));
    QCoreApplication::processEvents();

    auto *speed = page.findChild<GaugeWidget *>(QStringLiteral("speedGauge"));
    auto *rpm = page.findChild<GaugeWidget *>(QStringLiteral("rpmGauge"));
    auto *fuel = page.findChild<BatteryWidget *>(QStringLiteral("batteryWidget"));
    QVERIFY(speed && rpm && fuel);
    QCOMPARE(speed->value(), 120.0);
    QCOMPARE(rpm->value(), 3000.0);
    QCOMPARE(fuel->value(), 50.0);

    // 0x300 gear: byte bits1..3 = 3 -> 'D' on the gear label.
    car.onCanFrame(FrameId::Gear, QByteArray::fromHex("0600000000000000"));
    QCoreApplication::processEvents();
    auto *gear = page.findChild<QLabel *>(QStringLiteral("gearLabel"));
    QVERIFY(gear);
    QCOMPARE(gear->text(), QStringLiteral("D"));
}

void TestUiSmoke::climateRoundTripThroughUi()
{
    CarService car;
    ClimatePage page(&car);
    page.show();
    QCoreApplication::processEvents();
    QVERIFY(!car.hasPendingClimate());

    QSlider *temp = findSlider(page, 16); // 16..30 range
    QVERIFY(temp);
    QCOMPARE(temp->value(), 22); // default confirmed state

    // User dials to 24 -> after the 250 ms debounce the UI must issue a
    // climate command via CarService.
    temp->setValue(24);
    QTRY_VERIFY(car.hasPendingClimate());

    // Vehicle echoes the new set point with the ack bit set.
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
    QCOMPARE(car.climate().temperature, 24);
    QCOMPARE(temp->value(), 24); // UI refreshed from the confirmed state
}

void TestUiSmoke::climateCancelThenNewRequest()
{
    CarService car;
    ClimatePage page(&car);
    page.show();
    QCoreApplication::processEvents();
    auto *temp = findSlider(page, 16);
    auto *cancel = page.findChild<QPushButton *>(QStringLiteral("cancelPending"));
    auto *status = page.findChild<QLabel *>(QStringLiteral("statusPill"));
    QVERIFY(temp && cancel && status);
    QSignalSpy cancelledSpy(&car, &CarService::climateCommandCancelled);
    QSignalSpy timeoutSpy(&car, &CarService::climateAckTimeout);
    QSignalSpy ackSpy(&car, &CarService::climateAckOk);

    temp->setValue(24);
    QTRY_VERIFY(car.hasPendingClimate());
    QVERIFY(!temp->isEnabled());
    QVERIFY(cancel->isEnabled());
    cancel->click();
    QVERIFY(!car.hasPendingClimate());
    QCOMPARE(cancelledSpy.count(), 1);
    const quint32 oldId = cancelledSpy.at(0).at(0).toUInt();
    QVERIFY(oldId != 0);
    QVERIFY(temp->isEnabled());
    QVERIFY(!cancel->isEnabled());
    QCOMPARE(temp->value(), 22);
    QCOMPARE(status->text(), QStringLiteral("已取消"));
    QTest::qWait(CarService::kClimateAckTimeoutMs + 150);
    QCOMPARE(timeoutSpy.count(), 0);
    QCOMPARE(status->text(), QStringLiteral("已取消"));

    temp->setValue(28);
    QTRY_VERIFY(car.hasPendingClimate());
    QVERIFY(!temp->isEnabled());
    QVERIFY(cancel->isEnabled());

    // Simulate delayed local notifications for the cancelled request (and 0).
    // These IDs are local only; no transaction ID is added to the CAN frame.
    for (quint32 staleId : {quint32(0), oldId}) {
        for (const char *signal : {"climateAckOk", "climateAckTimeout",
                                   "climateCommandCancelled"}) {
            QVERIFY(QMetaObject::invokeMethod(&car, signal, Qt::DirectConnection,
                                               Q_ARG(quint32, staleId)));
            QVERIFY(!temp->isEnabled());
            QVERIFY(cancel->isEnabled());
            QCOMPARE(temp->value(), 28);
            QCOMPARE(status->text(), QStringLiteral("等待车辆确认..."));
        }
    }
    ackSpy.clear();
    timeoutSpy.clear();

    QHash<QString, double> echo = {
        {QStringLiteral("temp_set"), 24.0},
        {QStringLiteral("fan_speed"), 3.0},
        {QStringLiteral("blow_mode"), 0.0},
        {QStringLiteral("ac_on"), 1.0},
        {QStringLiteral("auto_mode"), 0.0},
        {QStringLiteral("ack"), 1.0}
    };
    QByteArray payload;
    QVERIFY(car.parser()->encode(FrameId::HVAC, echo, &payload));
    car.onCanFrame(FrameId::HVAC, payload);
    QVERIFY(car.hasPendingClimate());
    QCOMPARE(ackSpy.count(), 0);
    QCOMPARE(car.climate().temperature, 22);
    QCOMPARE(temp->value(), 28);
    QVERIFY(!temp->isEnabled());

    echo[QStringLiteral("temp_set")] = 28.0;
    QVERIFY(car.parser()->encode(FrameId::HVAC, echo, &payload));
    car.onCanFrame(FrameId::HVAC, payload);
    QVERIFY(!car.hasPendingClimate());
    QCOMPARE(ackSpy.count(), 1);
    const quint32 newId = ackSpy.at(0).at(0).toUInt();
    QVERIFY(newId != 0 && newId != oldId);
    QCOMPARE(car.climate().temperature, 28);
    QCOMPARE(temp->value(), 28);
    QVERIFY(temp->isEnabled());
    QVERIFY(!cancel->isEnabled());
    QCOMPARE(status->text(), QStringLiteral("已确认"));
    QTest::qWait(CarService::kClimateAckTimeoutMs + 150);
    QCOMPARE(timeoutSpy.count(), 0);
    QCOMPARE(status->text(), QStringLiteral("已确认"));

    // A notification without a UI pending request must also be ignored.
    car.climateAckTimeout(newId);
    car.climateCommandCancelled(newId);
    QCOMPARE(status->text(), QStringLiteral("已确认"));
    QCOMPARE(temp->value(), 28);
    timeoutSpy.clear();

    // A real timeout for the next UI request must still unlock and roll back.
    temp->setValue(26);
    QTRY_VERIFY(car.hasPendingClimate());
    QVERIFY(!temp->isEnabled());
    QVERIFY(timeoutSpy.wait(CarService::kClimateAckTimeoutMs + 800));
    QCOMPARE(timeoutSpy.count(), 1);
    QVERIFY(!car.hasPendingClimate());
    QVERIFY(temp->isEnabled());
    QVERIFY(!cancel->isEnabled());
    QCOMPARE(temp->value(), 28);
    QCOMPARE(status->text(), QStringLiteral("超时未确认，已回滚"));
}

void TestUiSmoke::climateRejectedSendDoesNotLockUi()
{
    CarService car;
    ClimatePage page(&car);
    page.show();
    QCoreApplication::processEvents();
    auto *temp = findSlider(page, 16);
    auto *cancel = page.findChild<QPushButton *>(QStringLiteral("cancelPending"));
    auto *status = page.findChild<QLabel *>(QStringLiteral("statusPill"));
    QVERIFY(temp && cancel && status);

    // A request owned by another caller makes the UI send return 0.
    ClimateState target = car.climate();
    target.temperature = 24;
    const quint32 externalId = car.sendClimateCommand(target);
    QVERIFY(externalId != 0);
    temp->setValue(28);
    QTRY_COMPARE(status->text(), QStringLiteral("发送失败，已回滚"));
    QVERIFY(car.hasPendingClimate());
    QVERIFY(temp->isEnabled());
    QVERIFY(!cancel->isEnabled());
    QCOMPARE(temp->value(), 22);
    QVERIFY(car.cancelClimateCommand(externalId));
    QCOMPARE(status->text(), QStringLiteral("发送失败，已回滚"));

    // The rejected send must not prevent the next UI request from succeeding.
    temp->setValue(26);
    QTRY_VERIFY(car.hasPendingClimate());
    QVERIFY(!temp->isEnabled());
    QVERIFY(cancel->isEnabled());
    const QHash<QString, double> echo = {
        {QStringLiteral("temp_set"), 26.0},
        {QStringLiteral("fan_speed"), 3.0},
        {QStringLiteral("blow_mode"), 0.0},
        {QStringLiteral("ac_on"), 1.0},
        {QStringLiteral("auto_mode"), 0.0},
        {QStringLiteral("ack"), 1.0}
    };
    QByteArray payload;
    QVERIFY(car.parser()->encode(FrameId::HVAC, echo, &payload));
    car.onCanFrame(FrameId::HVAC, payload);
    QVERIFY(!car.hasPendingClimate());
    QVERIFY(temp->isEnabled());
    QVERIFY(!cancel->isEnabled());
    QCOMPARE(temp->value(), 26);
    QCOMPARE(status->text(), QStringLiteral("已确认"));

    temp->setValue(30);
    QTRY_VERIFY(car.hasPendingClimate());
    car.stop();
    QVERIFY(!car.hasPendingClimate());
    QVERIFY(temp->isEnabled());
    QVERIFY(!cancel->isEnabled());
    QCOMPARE(temp->value(), 26);
    QCOMPARE(status->text(), QStringLiteral("已取消"));
}

void TestUiSmoke::simulatorDrivesInstrument()
{
    // Mirrors the app's --sim mode: no CAN socket, no vcan, yet the HMI must
    // react. Exercises CarService.setSimulatedMode + SignalSimulator end to end.
    CarService car;
    car.setSimulatedMode(true);
    QVERIFY(car.canConnected());

    InstrumentPage page(&car);
    page.show();
    QCoreApplication::processEvents();

    SignalSimulator sim(&car);
    sim.start();
    QTest::qWait(500); // a few 100 ms ticks
    sim.stop();

    QVERIFY(car.speed() > 0.0);
    auto *speed = page.findChild<GaugeWidget *>(QStringLiteral("speedGauge"));
    QVERIFY(speed);
    QCOMPARE(speed->value(), car.speed());
}

void TestUiSmoke::pagesRenderSomething()
{
    CarService car;
    MediaService media;

    InstrumentPage instrument(&car);
    QVERIFY(paintsContent(instrument));

    ClimatePage climate(&car);
    QVERIFY(paintsContent(climate));

    MediaPage mediaPage(&media);
    QVERIFY(paintsContent(mediaPage));
}

QTEST_MAIN(TestUiSmoke)
#include "tst_ui.moc"
