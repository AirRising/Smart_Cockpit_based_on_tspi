#include "ClimatePage.h"

#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QRadialGradient>
#include <QSlider>
#include <QVBoxLayout>
#include <QtMath>

#include "CarService.h"

namespace sc {

namespace {

QFrame *makeCard(QWidget *parent, const QString &title)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 16, 20, 20);
    layout->setSpacing(12);
    if (!title.isEmpty()) {
        auto *label = new QLabel(title, card);
        label->setObjectName(QStringLiteral("cardTitle"));
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        layout->addWidget(label);
    }
    return card;
}

// Big circular temperature dial: blue->amber arc showing the set point and a
// large center readout. Purely visual, driven by the temperature slider.
class TempDialWidget : public QWidget
{
public:
    explicit TempDialWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(150, 150);
        setMaximumSize(400, 400);
    }

    QSize sizeHint() const override { return QSize(400, 400); }

    void setTemperature(int temp)
    {
        m_temp = qBound(16, temp, 30);
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const qreal side = qMin<qreal>(width(), height()) - 8.0;
        const QRectF dial((width() - side) / 2.0, (height() - side) / 2.0, side, side);
        const qreal cx = dial.center().x();
        const qreal cy = dial.center().y();
        const qreal r = dial.width() / 2.0;

        // Outer rim.
        QRadialGradient rim(cx, cy, r);
        rim.setColorAt(0.0, QColor(0x23, 0x2D, 0x40));
        rim.setColorAt(0.7, QColor(0x16, 0x1C, 0x2A));
        rim.setColorAt(1.0, QColor(0x0A, 0x0D, 0x14));
        painter.setPen(Qt::NoPen);
        painter.setBrush(rim);
        painter.drawEllipse(dial);

        // Inner face.
        const QRectF face = dial.adjusted(r * 0.16, r * 0.16, -r * 0.16, -r * 0.16);
        painter.setPen(QPen(QColor(0x2A, 0x35, 0x50), 1.2));
        painter.setBrush(QColor(0x0C, 0x11, 0x1B));
        painter.drawEllipse(face);

        // Arc track.
        const qreal arcR = face.width() / 2.0 - r * 0.06;
        const QRectF arcRect(cx - arcR, cy - arcR, 2.0 * arcR, 2.0 * arcR);
        const qreal penW = r * 0.10;
        painter.setPen(QPen(QColor(0x22, 0x2B, 0x3D), penW, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(arcRect, 225 * 16, -270 * 16);

        // Cool -> warm value arc.
        const double frac = qBound(0.0, double(m_temp - 16) / 14.0, 1.0);
        const QColor valueColor = lerp(QColor(0x4D, 0xA8, 0xFF), QColor(0xFF, 0x6B, 0x35), frac);
        const int span = qRound(-frac * 270.0 * 16.0);
        painter.setPen(QPen(QColor(valueColor.red(), valueColor.green(), valueColor.blue(), 60),
                            penW * 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(arcRect, 225 * 16, span);
        painter.setPen(QPen(valueColor, penW, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(arcRect, 225 * 16, span);

        // Range labels.
        QFont range = font();
        range.setPixelSize(qMax(9, int(r * 0.10)));
        painter.setFont(range);
        painter.setPen(QColor(0x6F, 0x7A, 0x90));
        const qreal rr = arcR + r * 0.16;
        painter.drawText(QRectF(cx - 0.7071 * rr - 20, cy + 0.7071 * rr - 10, 40, 20),
                         Qt::AlignCenter, QStringLiteral("16"));
        painter.drawText(QRectF(cx + 0.7071 * rr - 20, cy + 0.7071 * rr - 10, 40, 20),
                         Qt::AlignCenter, QStringLiteral("30"));

        // Center readout.
        QFont big = font();
        big.setPixelSize(qMax(20, int(r * 0.30)));
        big.setWeight(QFont::Bold);
        painter.setFont(big);
        painter.setPen(QColor(0xEA, 0xF0, 0xF8));
        painter.drawText(QRectF(cx - r, cy - r * 0.62, 2.0 * r, r * 0.40),
                         Qt::AlignCenter, QString::number(m_temp));

        QFont unit = font();
        unit.setPixelSize(qMax(11, int(r * 0.14)));
        unit.setWeight(QFont::DemiBold);
        painter.setFont(unit);
        painter.setPen(QColor(0x8A, 0x94, 0xA8));
        painter.drawText(QRectF(cx - r, cy - r * 0.12, 2.0 * r, r * 0.24),
                         Qt::AlignCenter, QStringLiteral("\u00B0C"));
    }

private:
    static QColor lerp(const QColor &a, const QColor &b, double t)
    {
        return QColor(a.red() + int((b.red() - a.red()) * t),
                      a.green() + int((b.green() - a.green()) * t),
                      a.blue() + int((b.blue() - a.blue()) * t));
    }

    int m_temp = 22;
};

} // namespace

ClimatePage::ClimatePage(CarService *car, QWidget *parent)
    : QWidget(parent)
    , m_car(car)
{
    setObjectName(QStringLiteral("climatePage"));
    setStyleSheet(QStringLiteral("#climatePage { background: transparent; }"));

    // ----------------------------------------------------------- temperature
    auto *tempCard = makeCard(this, QStringLiteral("温度"));
    auto *tempLayout = qobject_cast<QVBoxLayout *>(tempCard->layout());

    auto *tempDial = new TempDialWidget(this);

    auto *tempMinus = new QPushButton(QStringLiteral("\u2212"), this);
    tempMinus->setObjectName(QStringLiteral("iconBtn"));
    tempMinus->setCursor(Qt::PointingHandCursor);
    auto *tempPlus = new QPushButton(QStringLiteral("+"), this);
    tempPlus->setObjectName(QStringLiteral("iconBtn"));
    tempPlus->setCursor(Qt::PointingHandCursor);

    auto *tempButtons = new QHBoxLayout;
    tempButtons->setSpacing(16);
    tempButtons->addWidget(tempMinus);
    tempButtons->addStretch();
    tempButtons->addWidget(tempPlus);

    m_tempSlider = new QSlider(Qt::Horizontal, this);
    m_tempSlider->setRange(16, 30);
    m_tempSlider->setValue(car->climate().temperature);

    connect(m_tempSlider, &QSlider::valueChanged, tempDial, &TempDialWidget::setTemperature);

    tempLayout->addWidget(tempDial, 0, Qt::AlignCenter);
    tempLayout->addLayout(tempButtons);
    tempLayout->addWidget(m_tempSlider);

    // ------------------------------------------------------------------- fan
    auto *fanCard = makeCard(this, QStringLiteral("风速"));
    auto *fanLayout = qobject_cast<QVBoxLayout *>(fanCard->layout());

    auto *fanRow = new QHBoxLayout;
    m_fanValue = new QLabel(this);
    m_fanValue->setObjectName(QStringLiteral("fanValue"));
    m_fanBar = new QProgressBar(this);
    m_fanBar->setObjectName(QStringLiteral("fanBar"));
    m_fanBar->setRange(0, 7);
    m_fanBar->setTextVisible(false);
    m_fanBar->setStyleSheet(QStringLiteral(
        "QProgressBar { background:#1A2233; border:none; border-radius:6px; height:12px; }"
        "QProgressBar::chunk { background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #CC8F00, stop:1 #FFB400); border-radius:6px; }"));
    fanRow->addWidget(m_fanValue);
    fanRow->addWidget(m_fanBar, 1);

    m_fanSlider = new QSlider(Qt::Horizontal, this);
    m_fanSlider->setRange(0, 7);
    m_fanSlider->setValue(car->climate().fanSpeed);

    fanLayout->addLayout(fanRow);
    fanLayout->addWidget(m_fanSlider);

    // -------------------------------------------------------------- blow mode
    auto *modeCard = makeCard(this, QStringLiteral("吹风模式"));
    auto *modeLayout = qobject_cast<QVBoxLayout *>(modeCard->layout());

    const QStringList modes = {QStringLiteral("面部出风"), QStringLiteral("腿部出风"),
                               QStringLiteral("除霜"), QStringLiteral("上下出风")};
    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);
    auto *modeGrid = new QGridLayout;
    modeGrid->setSpacing(8);
    for (int i = 0; i < modes.size(); ++i) {
        auto *button = new QPushButton(modes.at(i), this);
        button->setObjectName(QStringLiteral("mode"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        m_modeGroup->addButton(button, i);
        modeGrid->addWidget(button, i / 2, i % 2);
        connect(button, &QPushButton::clicked, this, [this, button]() {
            if (!button->isChecked()) // keep one mode selected
                button->setChecked(true);
            onControlChanged();
        });
    }
    modeLayout->addLayout(modeGrid);
    modeLayout->insertStretch(1);
    modeLayout->addStretch();

    // ------------------------------------------------------------- circulation
    auto *circCard = makeCard(this, QStringLiteral("循环模式"));
    auto *circLayout = qobject_cast<QVBoxLayout *>(circCard->layout());

    const QStringList circModes = {QStringLiteral("内循环"), QStringLiteral("外循环")};
    m_circGroup = new QButtonGroup(this);
    m_circGroup->setExclusive(true);
    for (int i = 0; i < circModes.size(); ++i) {
        auto *button = new QPushButton(circModes.at(i), this);
        button->setObjectName(QStringLiteral("mode"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        m_circGroup->addButton(button, i);
        circLayout->addWidget(button);
    }
    m_circGroup->button(0)->setChecked(true);
    circLayout->insertStretch(1);
    circLayout->addStretch();

    // ---------------------------------------------------------- toggles + status
    auto *acToggle = new QPushButton(QStringLiteral("AC 制冷"), this);
    acToggle->setObjectName(QStringLiteral("toggle"));
    acToggle->setCheckable(true);
    acToggle->setCursor(Qt::PointingHandCursor);
    auto *autoToggle = new QPushButton(QStringLiteral("自动"), this);
    autoToggle->setObjectName(QStringLiteral("toggle"));
    autoToggle->setCheckable(true);
    autoToggle->setCursor(Qt::PointingHandCursor);
    m_acToggle = acToggle;
    m_autoToggle = autoToggle;

    m_statusLabel = new QLabel(QStringLiteral("就绪"), this);
    m_statusLabel->setObjectName(QStringLiteral("statusPill"));
    m_statusLabel->setAlignment(Qt::AlignCenter);

    auto *cancelButton = new QPushButton(QStringLiteral("取消待处理"), this);
    cancelButton->setObjectName(QStringLiteral("danger"));
    cancelButton->setEnabled(false);
    connect(cancelButton, &QPushButton::clicked, this, [this, cancelButton]() {
        if (m_pending)
            onAckTimeout(0);
        cancelButton->setEnabled(false);
    });

    auto *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(10);
    bottomRow->addWidget(acToggle);
    bottomRow->addWidget(autoToggle);
    bottomRow->addStretch();
    bottomRow->addWidget(m_statusLabel);
    bottomRow->addWidget(cancelButton);

    // ----------------------------------------------------------------- layout
    auto *leftColumn = new QVBoxLayout;
    leftColumn->setSpacing(18);
    leftColumn->addWidget(tempCard, 3);
    leftColumn->addWidget(fanCard, 1);

    auto *rightColumn = new QVBoxLayout;
    rightColumn->setSpacing(18);
    rightColumn->addWidget(modeCard, 1);
    rightColumn->addWidget(circCard, 1);

    auto *cardsRow = new QHBoxLayout;
    cardsRow->setSpacing(18);
    cardsRow->addLayout(leftColumn, 3);
    cardsRow->addLayout(rightColumn, 2);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 18, 24, 18);
    mainLayout->setSpacing(18);
    mainLayout->addLayout(cardsRow, 1);
    mainLayout->addLayout(bottomRow);

    // ---------------------------------------------------------------- wiring
    connect(tempMinus, &QPushButton::clicked, this, [this]() {
        m_tempSlider->setValue(m_tempSlider->value() - 1);
    });
    connect(tempPlus, &QPushButton::clicked, this, [this]() {
        m_tempSlider->setValue(m_tempSlider->value() + 1);
    });

    connect(m_tempSlider, &QSlider::valueChanged, this, &ClimatePage::onControlChanged);
    connect(m_fanSlider, &QSlider::valueChanged, this, &ClimatePage::onControlChanged);
    connect(acToggle, &QPushButton::toggled, this, &ClimatePage::onControlChanged);
    connect(autoToggle, &QPushButton::toggled, this, &ClimatePage::onControlChanged);

    connect(car, &CarService::climateStateChanged, this, &ClimatePage::onClimateStateChanged);
    connect(car, &CarService::climateAckOk, this, &ClimatePage::onAckOk);
    connect(car, &CarService::climateAckTimeout, this, &ClimatePage::onAckTimeout);

    // Initial state.
    setPanelEnabled(true, QStringLiteral("就绪"));
    applyControls(car->climate());
}

sc::ClimateState ClimatePage::readControls() const
{
    sc::ClimateState state;
    state.temperature = m_tempSlider->value();
    state.fanSpeed = m_fanSlider->value();
    state.blowMode = qMax(0, m_modeGroup->checkedId());
    state.acOn = m_acToggle->isChecked();
    state.autoMode = m_autoToggle->isChecked();
    return state;
}

void ClimatePage::applyControls(const sc::ClimateState &state)
{
    m_syncing = true;
    m_tempSlider->setValue(state.temperature);
    m_fanSlider->setValue(state.fanSpeed);
    m_acToggle->setChecked(state.acOn);
    m_autoToggle->setChecked(state.autoMode);
    if (auto *button = m_modeGroup->button(state.blowMode))
        button->setChecked(true);
    updateReadouts();
    m_syncing = false;
}

void ClimatePage::setPanelEnabled(bool enabled, const QString &statusText)
{
    m_tempSlider->setEnabled(enabled);
    m_fanSlider->setEnabled(enabled);
    m_acToggle->setEnabled(enabled);
    m_autoToggle->setEnabled(enabled);
    for (QAbstractButton *button : m_modeGroup->buttons())
        button->setEnabled(enabled);

    m_statusLabel->setText(statusText);
    m_statusLabel->setStyleSheet(enabled
        ? QStringLiteral("background:#163B26; color:#7FE6A0; border:1px solid #2FBF71;"
                         "border-radius:12px; padding:6px 14px; font-weight:700; font-size:12px;")
        : QStringLiteral("background:#3D2F12; color:#FFC24B; border:1px solid #FFB400;"
                         "border-radius:12px; padding:6px 14px; font-weight:700; font-size:12px;"));
}

void ClimatePage::updateReadouts()
{
    m_fanValue->setText(QStringLiteral("<span style='font-size:26px; font-weight:700; "
                                       "color:#FFC24B;'>%1</span>")
                            .arg(m_fanSlider->value()));
    m_fanBar->setValue(m_fanSlider->value());
}

void ClimatePage::onControlChanged()
{
    if (m_syncing || m_pending)
        return;

    updateReadouts();
    m_lastSent = readControls();
    m_car->sendClimateCommand(m_lastSent);
    m_pending = true;
    setPanelEnabled(false, QStringLiteral("等待车辆确认..."));
}

void ClimatePage::onClimateStateChanged(const sc::ClimateState &state)
{
    if (!m_pending)
        applyControls(state);
}

void ClimatePage::onAckOk(quint32 requestId)
{
    Q_UNUSED(requestId)
    m_pending = false;
    setPanelEnabled(true, QStringLiteral("已确认"));
    applyControls(m_car->climate());
}

void ClimatePage::onAckTimeout(quint32 requestId)
{
    Q_UNUSED(requestId)
    m_pending = false;
    setPanelEnabled(true, QStringLiteral("超时未确认，已回滚"));
    applyControls(m_car->climate());
}

} // namespace sc
