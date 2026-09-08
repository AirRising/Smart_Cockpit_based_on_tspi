#include <QtTest>

#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QSlider>
#include <QWidget>

#include "CameraService.h"
#include "CarService.h"
#include "ClimatePage.h"
#include "GaugeWidget.h"
#include "InstrumentPage.h"
#include "MediaPage.h"
#include "MediaService.h"
#include "ReverseCameraPage.h"
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
    void simulatorDrivesInstrument();
    void pagesRenderSomething();
    void reversePageRendersSomething();
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
    auto *fuel = page.findChild<GaugeWidget *>(QStringLiteral("fuelGauge"));
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

    // User dials to 24 -> the UI must issue a climate command via CarService.
    temp->setValue(24);
    QCoreApplication::processEvents();
    QVERIFY(car.hasPendingClimate());

    // Vehicle echoes the new set point with the ack bit set.
    QHash<QString, double> echo;
    echo.insert(QStringLiteral("temp_set"), 24.0);
    echo.insert(QStringLiteral("fan_speed"), 3.0);
    echo.insert(QStringLiteral("blow_mode"), 0.0);
    echo.insert(QStringLiteral("ac_on"), 0.0);
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

// The reverse page hosts a QOpenGLWidget on GL-capable platforms. Headless
// (Qt6 "offscreen") runs must fall back to the software renderer instead of
// crashing inside Qt's backing-store RHI flush.
void TestUiSmoke::reversePageRendersSomething()
{
    CarService car;
    CameraService camera;
    ReverseCameraPage page(&camera, &car);
    QVERIFY(paintsContent(page));
}

QTEST_MAIN(TestUiSmoke)
#include "tst_ui.moc"
