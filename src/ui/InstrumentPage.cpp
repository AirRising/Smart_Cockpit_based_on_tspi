#include "InstrumentPage.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include "BatteryWidget.h"
#include "CarService.h"
#include "GaugeWidget.h"
#include "Theme.h"

namespace sc {

namespace {

// Card with an optional title label.
QFrame *makeCard(QWidget *parent, const QString &title)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);
    if (!title.isEmpty()) {
        auto *label = new QLabel(title, card);
        label->setObjectName(QStringLiteral("cardTitle"));
        layout->addWidget(label);
    }
    return card;
}

// Warning-lamp chip: rounded outline chip with a short text. Off state is a
// thin disabled-colour outline, on state is a solid semantic colour fill.
class LampWidget : public QWidget
{
public:
    LampWidget(const QString &text, const QColor &onColor, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_text(text)
        , m_onColor(onColor)
    {
        setFixedSize(72, 32);
    }

    void setOn(bool on)
    {
        if (m_on == on)
            return;
        m_on = on;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF chip = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        if (m_on) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_onColor);
        } else {
            painter.setPen(QPen(QColor(QLatin1String(Theme::Disabled)), 1.0));
            painter.setBrush(Qt::NoBrush);
        }
        painter.drawRoundedRect(chip, Theme::RadiusSmall, Theme::RadiusSmall);

        QFont f = font();
        f.setPixelSize(13);
        f.setWeight(QFont::DemiBold);
        painter.setFont(f);
        painter.setPen(m_on ? QColor(QLatin1String(Theme::Bg))
                            : QColor(QLatin1String(Theme::TextSecondary)));
        painter.drawText(rect(), Qt::AlignCenter, m_text);
    }

private:
    QString m_text;
    QColor m_onColor;
    bool m_on = false;
};

void setLamp(QWidget *lamp, bool on)
{
    static_cast<LampWidget *>(lamp)->setOn(on);
}

} // namespace

InstrumentPage::InstrumentPage(CarService *car, QWidget *parent)
    : QWidget(parent)
    , m_car(car)
{
    setObjectName(QStringLiteral("instrumentPage"));

    m_speedGauge = new GaugeWidget(this);
    m_speedGauge->setObjectName(QStringLiteral("speedGauge"));
    m_speedGauge->configure(0, 240, QStringLiteral("km/h"), 200, 12);
    m_speedGauge->setLabel(QStringLiteral("车速"));

    m_rpmGauge = new GaugeWidget(this);
    m_rpmGauge->setObjectName(QStringLiteral("rpmGauge"));
    m_rpmGauge->configure(0, 8000, QStringLiteral("rpm"), 6000, 8);
    m_rpmGauge->setLabel(QStringLiteral("电机转速"));

    m_battery = new BatteryWidget(this);
    m_battery->setObjectName(QStringLiteral("batteryWidget"));
    m_battery->setLabel(QStringLiteral("电量"));
    m_battery->setFixedHeight(112);

    // ------------------------------------------------------------ gauges
    auto *speedCard = makeCard(this, QString());
    qobject_cast<QVBoxLayout *>(speedCard->layout())->addWidget(m_speedGauge, 1);

    auto *rpmCard = makeCard(this, QString());
    qobject_cast<QVBoxLayout *>(rpmCard->layout())->addWidget(m_rpmGauge, 1);

    // -------------------------------------------------------- status card
    auto *statusCard = makeCard(this, QStringLiteral("车辆状态"));
    auto *statusLayout = qobject_cast<QVBoxLayout *>(statusCard->layout());

    auto *gearCaption = new QLabel(QStringLiteral("挡位"), statusCard);
    gearCaption->setObjectName(QStringLiteral("cardTitle"));
    gearCaption->setAlignment(Qt::AlignCenter);

    m_gearLabel = new QLabel(QStringLiteral("P"), statusCard);
    m_gearLabel->setObjectName(QStringLiteral("gearLabel"));
    m_gearLabel->setAlignment(Qt::AlignCenter);
    m_gearLabel->setFixedSize(96, 96);

    auto *gearBlock = new QVBoxLayout;
    gearBlock->setSpacing(8);
    gearBlock->addWidget(gearCaption);
    gearBlock->addWidget(m_gearLabel, 0, Qt::AlignCenter);

    const QColor warn(QLatin1String(Theme::Warn));
    const QColor err(QLatin1String(Theme::Err));
    m_leftLamp = new LampWidget(QStringLiteral("◀"), warn, statusCard);
    m_rightLamp = new LampWidget(QStringLiteral("▶"), warn, statusCard);
    m_hazardLamp = new LampWidget(QStringLiteral("▲"), warn, statusCard);
    m_engineLamp = new LampWidget(QStringLiteral("电机"), err, statusCard);
    m_absLamp = new LampWidget(QStringLiteral("ABS"), err, statusCard);
    m_airbagLamp = new LampWidget(QStringLiteral("气囊"), err, statusCard);

    auto *lampGrid = new QGridLayout;
    lampGrid->setSpacing(8);
    lampGrid->addWidget(m_leftLamp, 0, 0);
    lampGrid->addWidget(m_rightLamp, 0, 1);
    lampGrid->addWidget(m_hazardLamp, 0, 2);
    lampGrid->addWidget(m_engineLamp, 1, 0);
    lampGrid->addWidget(m_absLamp, 1, 1);
    lampGrid->addWidget(m_airbagLamp, 1, 2);

    auto *statusRow = new QHBoxLayout;
    statusRow->setSpacing(16);
    statusRow->addStretch(1);
    statusRow->addLayout(gearBlock, 0);
    statusRow->addStretch(1);
    statusRow->addWidget(m_battery, 2);
    statusRow->addStretch(1);
    statusRow->addLayout(lampGrid, 0);
    statusRow->addStretch(1);
    statusLayout->addStretch(1);
    statusLayout->addLayout(statusRow);
    statusLayout->addStretch(1);

    // ------------------------------------------------------------- layout
    // Vertical three sections sized by stretch (portrait 800x1280 reference:
    // ~511 / ~331 / ~220 px inside ~1087 px of content height).
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 16, 24, 16);
    mainLayout->setSpacing(16);
    mainLayout->addWidget(speedCard, 47);
    mainLayout->addWidget(rpmCard, 30);
    mainLayout->addWidget(statusCard, 20);

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
    m_battery->setValue(percent);
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

} // namespace sc
