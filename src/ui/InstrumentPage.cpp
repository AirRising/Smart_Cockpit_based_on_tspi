#include "InstrumentPage.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "CarService.h"
#include "GaugeWidget.h"

namespace sc {

namespace {

// Card with an optional title label; returns a ready QVBoxLayout for content.
QVBoxLayout *makeCard(QWidget *parent, const QString &title)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 16, 20, 20);
    layout->setSpacing(12);
    if (!title.isEmpty()) {
        auto *label = new QLabel(title, card);
        label->setObjectName(QStringLiteral("cardTitle"));
        layout->addWidget(label);
    }
    return layout;
}

} // namespace

InstrumentPage::InstrumentPage(CarService *car, QWidget *parent)
    : QWidget(parent)
    , m_car(car)
{
    setObjectName(QStringLiteral("instrumentPage"));
    setStyleSheet(QStringLiteral("#instrumentPage { background: transparent; }"));

    m_speedGauge = new GaugeWidget(this);
    m_speedGauge->setObjectName(QStringLiteral("speedGauge"));
    m_speedGauge->configure(0, 240, QStringLiteral("km/h"), 200, 12);
    m_speedGauge->setLabel(QStringLiteral("车速"));

    m_rpmGauge = new GaugeWidget(this);
    m_rpmGauge->setObjectName(QStringLiteral("rpmGauge"));
    m_rpmGauge->configure(0, 8000, QStringLiteral("rpm"), 6000, 8);
    m_rpmGauge->setLabel(QStringLiteral("电机转速"));

    m_fuelGauge = new GaugeWidget(this);
    m_fuelGauge->setObjectName(QStringLiteral("fuelGauge"));
    m_fuelGauge->configure(0, 100, QStringLiteral("%"), 15, 10, 0);
    m_fuelGauge->setLabel(QStringLiteral("电量"));
    m_fuelGauge->setFixedSize(150, 150);

    auto *speedCard = makeCard(this, QString());
    speedCard->addWidget(m_speedGauge, 1);

    auto *rpmCard = makeCard(this, QString());
    rpmCard->addWidget(m_rpmGauge, 1);

    auto *statusLayout = makeCard(this, QStringLiteral("车辆状态"));

    auto *gearCaption = new QLabel(QStringLiteral("挡位"), this);
    gearCaption->setObjectName(QStringLiteral("cardTitle"));
    gearCaption->setAlignment(Qt::AlignCenter);

    m_gearLabel = new QLabel(QStringLiteral("P"), this);
    m_gearLabel->setObjectName(QStringLiteral("gearLabel"));
    m_gearLabel->setAlignment(Qt::AlignCenter);
    m_gearLabel->setFixedSize(84, 84);
    m_gearLabel->setStyleSheet(QStringLiteral(
        "background:#1B1F2E; border:2px solid #FFB400; border-radius:42px;"
        "color:#FFC24B; font-size:40px; font-weight:800;"));

    auto *gearBlock = new QVBoxLayout;
    gearBlock->setSpacing(8);
    gearBlock->addWidget(gearCaption);
    gearBlock->addWidget(m_gearLabel, 0, Qt::AlignCenter);

    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(24);
    topRow->addLayout(gearBlock, 0);
    topRow->addWidget(m_fuelGauge, 1, Qt::AlignCenter);
    statusLayout->addLayout(topRow);

    auto makeLamp = [this](const QString &text, int width = 54) {
        auto *lamp = new QLabel(text, this);
        lamp->setAlignment(Qt::AlignCenter);
        lamp->setFixedSize(width, 30);
        setLamp(lamp, false);
        return lamp;
    };
    m_leftLamp = makeLamp(QStringLiteral("\u25C0"));
    m_rightLamp = makeLamp(QStringLiteral("\u25B6"));
    m_hazardLamp = makeLamp(QStringLiteral("\u25B2"));
    m_engineLamp = makeLamp(QStringLiteral("电机"));
    m_absLamp = makeLamp(QStringLiteral("ABS"));
    m_airbagLamp = makeLamp(QStringLiteral("气囊"), 70);

    auto *lampGrid = new QGridLayout;
    lampGrid->setSpacing(8);
    lampGrid->addWidget(m_leftLamp, 0, 0);
    lampGrid->addWidget(m_rightLamp, 0, 1);
    lampGrid->addWidget(m_hazardLamp, 0, 2);
    lampGrid->addWidget(m_engineLamp, 1, 0);
    lampGrid->addWidget(m_absLamp, 1, 1);
    lampGrid->addWidget(m_airbagLamp, 1, 2);
    lampGrid->setColumnStretch(3, 1);
    statusLayout->addLayout(lampGrid);
    statusLayout->addStretch(1);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(24, 18, 24, 18);
    mainLayout->setSpacing(18);
    mainLayout->addWidget(speedCard->parentWidget(), 3);
    mainLayout->addWidget(rpmCard->parentWidget(), 3);
    mainLayout->addWidget(statusLayout->parentWidget(), 2);

    connect(car, &CarService::speedChanged, this, &InstrumentPage::onSpeedChanged);
    connect(car, &CarService::rpmChanged, this, &InstrumentPage::onRpmChanged);
    connect(car, &CarService::fuelChanged, this, &InstrumentPage::onFuelChanged);
    connect(car, &CarService::indicatorsChanged, this, &InstrumentPage::onIndicators);
    connect(car, &CarService::milsChanged, this, &InstrumentPage::onMils);
    connect(car, &CarService::gearChanged, this, &InstrumentPage::onGearChanged);

    // Initial state.
    onSpeedChanged(car->speed());
    onRpmChanged(car->rpm());
    onFuelChanged(car->fuel());
    onIndicators(false, false, false);
    onMils(false, false, false);
    onGearChanged(car->gearPosition());
}

void InstrumentPage::onSpeedChanged(double kmh)
{
    m_speedGauge->setValue(kmh);
}

void InstrumentPage::onRpmChanged(int rpm)
{
    m_rpmGauge->setValue(rpm);
}

void InstrumentPage::onFuelChanged(double percent)
{
    m_fuelGauge->setValue(percent);
}

void InstrumentPage::onIndicators(bool left, bool right, bool hazard)
{
    setLamp(m_leftLamp, left || hazard);
    setLamp(m_rightLamp, right || hazard);
    setLamp(m_hazardLamp, hazard);
}

void InstrumentPage::onMils(bool engine, bool abs, bool airbag)
{
    setLamp(m_engineLamp, engine);
    setLamp(m_absLamp, abs);
    setLamp(m_airbagLamp, airbag);
}

void InstrumentPage::onGearChanged(int position)
{
    static const char *gearNames[] = {"P", "R", "N", "D"};
    m_gearLabel->setText(QLatin1String(gearNames[qBound(0, position, 3)]));
}

void InstrumentPage::setLamp(QLabel *lamp, bool on)
{
    if (on) {
        lamp->setStyleSheet(QStringLiteral(
            "background:#7A3B00; color:#FFC24B; border:1px solid #FFB400;"
            "border-radius:12px; font-weight:700;"));
    } else {
        lamp->setStyleSheet(QStringLiteral(
            "background:#141A26; color:#4A5468; border:1px solid #1C2434;"
            "border-radius:12px;"));
    }
}

} // namespace sc
