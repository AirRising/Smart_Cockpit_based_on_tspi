#include "ClimatePage.h"

#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>
#include <QtMath>

#include "CarService.h"
#include "Theme.h"

namespace sc {

namespace {

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
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        layout->addWidget(label);
    }
    return card;
}

// Big circular temperature dial with a cached static layer: the dial face,
// arc track and range labels are rendered once per resize; each paint only
// draws the value arc (two-state cool/warm colour) and the centre readout.
// Purely visual, driven by the temperature slider.
class TempDialWidget : public QWidget
{
public:
    explicit TempDialWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(200, 200);
        setMaximumSize(360, 360);
    }

    QSize sizeHint() const override { return QSize(320, 320); }

    void setTemperature(int temp)
    {
        m_temp = qBound(16, temp, 30);
        update();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        rebuildCache();
        QWidget::resizeEvent(event);
    }

    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        if (m_staticLayer.isNull())
            rebuildCache();
        if (m_staticLayer.isNull())
            return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawPixmap(0, 0, m_staticLayer);

        // Value arc: cool <= 20C, warm > 20C. No interpolation, no glow.
        const double frac = qBound(0.0, double(m_temp - 16) / 14.0, 1.0);
        if (frac > 0.0) {
            painter.setPen(QPen(valueColor(), m_arcPenWidth,
                                Qt::SolidLine, Qt::RoundCap));
            painter.setBrush(Qt::NoBrush);
            painter.drawArc(m_arcRect, 225 * 16, qRound(-frac * 270.0 * 16.0));
        }

        // Centre readout (monospaced digits -> no width jitter).
        painter.setFont(m_valueFont);
        painter.setPen(QColor(QLatin1String(Theme::TextPrimary)));
        painter.drawText(m_valueRect, Qt::AlignCenter, QString::number(m_temp));

        painter.setFont(m_unitFont);
        painter.setPen(QColor(QLatin1String(Theme::TextSecondary)));
        painter.drawText(m_unitRect, Qt::AlignCenter, QStringLiteral("°C"));
    }

private:
    QColor valueColor() const
    {
        return (m_temp <= 20) ? QColor(QLatin1String(Theme::Cool))
                              : QColor(QLatin1String(Theme::Warm));
    }

    void rebuildCache()
    {
        m_staticLayer = QPixmap();
        if (width() < 40 || height() < 40)
            return;

        const qreal dpr = devicePixelRatioF();
        m_staticLayer = QPixmap(size() * dpr);
        m_staticLayer.setDevicePixelRatio(dpr);
        m_staticLayer.fill(Qt::transparent);

        const qreal side = qMin<qreal>(width(), height()) - 8.0;
        const QRectF dial((width() - side) / 2.0, (height() - side) / 2.0, side, side);
        const qreal cx = dial.center().x();
        const qreal cy = dial.center().y();
        const qreal r = dial.width() / 2.0;

        const QRectF face = dial.adjusted(r * 0.14, r * 0.14, -r * 0.14, -r * 0.14);
        m_arcPenWidth = qMax(4.0, r * 0.075);
        const qreal arcR = face.width() / 2.0 - m_arcPenWidth / 2.0 - 2.0;
        m_arcRect = QRectF(cx - arcR, cy - arcR, 2.0 * arcR, 2.0 * arcR);

        m_valueFont = Theme::monoFont(qMax(22, int(r * 0.32)), QFont::Bold);
        m_unitFont = Theme::monoFont(qMax(11, int(r * 0.13)), QFont::DemiBold);
        m_valueRect = QRectF(cx - r, cy - r * 0.40, 2.0 * r, r * 0.46);
        m_unitRect = QRectF(cx - r, cy + r * 0.10, 2.0 * r, r * 0.22);

        // --------------------------------------------------- static layer
        QPainter painter(&m_staticLayer);
        painter.setRenderHint(QPainter::Antialiasing);

        // Dial face.
        painter.setPen(QPen(QColor(QLatin1String(Theme::CardBorder)), 1.2));
        painter.setBrush(QColor(QLatin1String(Theme::Card)));
        painter.drawEllipse(face);

        // Arc track.
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(QLatin1String(Theme::CardBorder)), m_arcPenWidth,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(m_arcRect, 225 * 16, -270 * 16);

        // Range labels.
        QFont rangeFont = Theme::monoFont(qMax(9, int(r * 0.09)), QFont::DemiBold);
        painter.setFont(rangeFont);
        painter.setPen(QColor(QLatin1String(Theme::TextSecondary)));
        const qreal rr = arcR + r * 0.14;
        painter.drawText(QRectF(cx - 0.7071 * rr - 20, cy + 0.7071 * rr - 10, 40, 20),
                         Qt::AlignCenter, QStringLiteral("16"));
        painter.drawText(QRectF(cx + 0.7071 * rr - 20, cy + 0.7071 * rr - 10, 40, 20),
                         Qt::AlignCenter, QStringLiteral("30"));
    }

    int m_temp = 22;

    QPixmap m_staticLayer;
    QRectF m_arcRect;
    qreal m_arcPenWidth = 4.0;
    QFont m_valueFont;
    QFont m_unitFont;
    QRectF m_valueRect;
    QRectF m_unitRect;
};

// Checkable button with a QPainter-drawn HVAC glyph above a short text.
// Only painting is customised: checkable state, mouse handling and clicked
// are entirely inherited from QPushButton.
class IconButton : public QPushButton
{
public:
    enum class IconKind { Face, Feet, Defrost, FaceFeet, Recirc, Fresh };

    IconButton(const QString &text, IconKind kind, QWidget *parent = nullptr)
        : QPushButton(text, parent)
        , m_kind(kind)
    {
        setMinimumSize(96, 56);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        buildGlyph();
    }

    QSize sizeHint() const override { return QSize(120, 56); }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool checked = isChecked();
        const QRectF chip = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        if (checked) {
            painter.setPen(QPen(QColor(QLatin1String(Theme::Accent)), 1.2));
            painter.setBrush(QColor(QLatin1String(Theme::Bg)));
        } else {
            painter.setPen(QPen(QColor(QLatin1String(Theme::CardBorder)), 1.0));
            painter.setBrush(Qt::NoBrush);
        }
        painter.drawRoundedRect(chip, Theme::RadiusSmall, Theme::RadiusSmall);

        const QColor fg = checked ? QColor(QLatin1String(Theme::Cool))
                                  : QColor(QLatin1String(Theme::TextSecondary));

        // Glyph: pre-built 24x24 unit-space path, scaled into the icon box.
        const QRectF iconRect((width() - 24.0) / 2.0, 6.0, 24.0, 24.0);
        painter.save();
        painter.translate(iconRect.topLeft());
        painter.scale(iconRect.width() / 24.0, iconRect.height() / 24.0);
        painter.setPen(QPen(fg, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(m_glyph);
        painter.restore();

        QFont f = font();
        f.setPixelSize(12);
        f.setWeight(QFont::DemiBold);
        painter.setFont(f);
        painter.setPen(fg);
        painter.drawText(QRectF(0, height() - 24.0, width(), 20.0),
                         Qt::AlignCenter, text());
    }

private:
    void addArrow(qreal x1, qreal y1, qreal x2, qreal y2)
    {
        m_glyph.moveTo(x1, y1);
        m_glyph.lineTo(x2, y2);
        const qreal angle = qAtan2(y2 - y1, x2 - x1);
        for (const qreal spread : {0.45, -0.45}) {
            m_glyph.moveTo(x2, y2);
            m_glyph.lineTo(x2 - 3.0 * qCos(angle + spread),
                           y2 - 3.0 * qSin(angle + spread));
        }
    }

    void addPerson()
    {
        m_glyph.addEllipse(QRectF(9.0, 2.5, 6.0, 6.0)); // head
        m_glyph.moveTo(7.0, 20.0);                      // torso
        m_glyph.lineTo(7.0, 15.0);
        m_glyph.quadTo(7.0, 11.5, 12.0, 11.5);
        m_glyph.quadTo(17.0, 11.5, 17.0, 15.0);
        m_glyph.lineTo(17.0, 20.0);
    }

    void addCabin()
    {
        m_glyph.addRoundedRect(QRectF(5.0, 6.5, 14.0, 11.0), 2.0, 2.0);
    }

    // All glyphs live in a 24x24 unit box; paintEvent only scales and draws.
    void buildGlyph()
    {
        switch (m_kind) {
        case IconKind::Face:
            addPerson();
            addArrow(1.5, 5.5, 5.5, 5.5);
            break;
        case IconKind::Feet:
            addPerson();
            addArrow(1.5, 20.0, 5.5, 20.0);
            break;
        case IconKind::FaceFeet:
            addPerson();
            addArrow(1.5, 5.5, 5.5, 5.5);
            addArrow(1.5, 20.0, 5.5, 20.0);
            break;
        case IconKind::Defrost:
            // Windshield trapezoid with three rising wavy streams.
            m_glyph.moveTo(3.5, 18.5);
            m_glyph.lineTo(7.5, 5.5);
            m_glyph.lineTo(16.5, 5.5);
            m_glyph.lineTo(20.5, 18.5);
            for (const qreal x : {9.0, 12.0, 15.0}) {
                m_glyph.moveTo(x, 16.0);
                m_glyph.cubicTo(x - 2.0, 13.5, x + 2.0, 11.5, x, 9.0);
            }
            break;
        case IconKind::Recirc:
            addCabin();
            // Circular arrow inside the cabin.
            m_glyph.arcTo(QRectF(8.5, 9.0, 7.0, 7.0), 40.0, 280.0);
            addArrow(14.0, 15.6, 14.9, 12.6);
            break;
        case IconKind::Fresh:
            addCabin();
            // Arrow entering the cabin from outside.
            addArrow(1.0, 12.0, 8.5, 12.0);
            break;
        }
    }

    IconKind m_kind;
    QPainterPath m_glyph;
};

} // namespace

ClimatePage::ClimatePage(CarService *car, QWidget *parent)
    : QWidget(parent)
    , m_car(car)
{
    setObjectName(QStringLiteral("climatePage"));

    // ----------------------------------------------------------- temperature
    auto *tempCard = makeCard(this, QStringLiteral("温度"));
    auto *tempLayout = qobject_cast<QVBoxLayout *>(tempCard->layout());

    auto *tempDial = new TempDialWidget(this);

    auto *tempMinus = new QPushButton(QStringLiteral("−"), this);
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

    tempLayout->addWidget(tempDial, 1, Qt::AlignCenter);
    tempLayout->addLayout(tempButtons);
    tempLayout->addWidget(m_tempSlider);

    // ------------------------------------------------------------------- fan
    auto *fanCard = makeCard(this, QStringLiteral("风速"));
    auto *fanLayout = qobject_cast<QVBoxLayout *>(fanCard->layout());

    auto *fanRow = new QHBoxLayout;
    m_fanValue = new QLabel(this);
    m_fanValue->setObjectName(QStringLiteral("fanValue"));
    m_fanValue->setFont(Theme::monoFont(26, QFont::Bold)); // one-time setup
    m_fanBar = new QProgressBar(this);
    m_fanBar->setObjectName(QStringLiteral("fanBar"));
    m_fanBar->setRange(0, 7);
    m_fanBar->setTextVisible(false);
    fanRow->addWidget(m_fanValue);
    fanRow->addWidget(m_fanBar, 1);

    m_fanSlider = new QSlider(Qt::Horizontal, this);
    m_fanSlider->setRange(0, 7);
    m_fanSlider->setValue(car->climate().fanSpeed);

    fanLayout->addStretch(1);
    fanLayout->addLayout(fanRow);
    fanLayout->addWidget(m_fanSlider);
    fanLayout->addStretch(1);

    // -------------------------------------------------------------- blow mode
    auto *modeCard = makeCard(this, QStringLiteral("吹风模式"));
    auto *modeLayout = qobject_cast<QVBoxLayout *>(modeCard->layout());

    const QStringList modes = {QStringLiteral("面部出风"), QStringLiteral("腿部出风"),
                               QStringLiteral("除霜"), QStringLiteral("上下出风")};
    const IconButton::IconKind modeIcons[] = {
        IconButton::IconKind::Face, IconButton::IconKind::Feet,
        IconButton::IconKind::Defrost, IconButton::IconKind::FaceFeet};
    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);
    auto *modeGrid = new QGridLayout;
    modeGrid->setSpacing(8);
    for (int i = 0; i < modes.size(); ++i) {
        auto *button = new IconButton(modes.at(i), modeIcons[i], this);
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
    modeLayout->addStretch(1);
    modeLayout->addLayout(modeGrid);
    modeLayout->addStretch(1);

    // ------------------------------------------------------------- circulation
    auto *circCard = makeCard(this, QStringLiteral("循环模式"));
    auto *circLayout = qobject_cast<QVBoxLayout *>(circCard->layout());

    const QStringList circModes = {QStringLiteral("内循环"), QStringLiteral("外循环")};
    const IconButton::IconKind circIcons[] = {
        IconButton::IconKind::Recirc, IconButton::IconKind::Fresh};
    m_circGroup = new QButtonGroup(this);
    m_circGroup->setExclusive(true);
    auto *circColumn = new QVBoxLayout;
    circColumn->setSpacing(8);
    for (int i = 0; i < circModes.size(); ++i) {
        auto *button = new IconButton(circModes.at(i), circIcons[i], this);
        button->setObjectName(QStringLiteral("mode"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        m_circGroup->addButton(button, i);
        circColumn->addWidget(button);
    }
    m_circGroup->button(0)->setChecked(true);
    circLayout->addStretch(1);
    circLayout->addLayout(circColumn);
    circLayout->addStretch(1);

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
    cancelButton->setObjectName(QStringLiteral("cancelPending"));
    cancelButton->setEnabled(false);
    m_cancelButton = cancelButton;
    connect(cancelButton, &QPushButton::clicked, this, [this]() {
        if (m_pendingRequestId)
            m_car->cancelClimateCommand(m_pendingRequestId);
    });

    auto *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(8);
    bottomRow->addWidget(acToggle);
    bottomRow->addWidget(autoToggle);
    bottomRow->addStretch();
    bottomRow->addWidget(m_statusLabel);
    bottomRow->addWidget(cancelButton);

    // ----------------------------------------------------------------- layout
    auto *middleRow = new QHBoxLayout;
    middleRow->setSpacing(16);
    middleRow->addWidget(modeCard, 3);
    middleRow->addWidget(circCard, 2);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 16, 24, 16);
    mainLayout->setSpacing(16);
    mainLayout->addWidget(tempCard, 42);
    mainLayout->addWidget(fanCard, 18);
    mainLayout->addLayout(middleRow, 24);
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
    connect(car, &CarService::climateCommandCancelled, this, &ClimatePage::onCommandCancelled);

    // Debounce: rapid control changes restart this timer; the command is only
    // sent 250 ms after the user stops adjusting.
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(250);
    connect(&m_debounceTimer, &QTimer::timeout, this, &ClimatePage::issueClimateCommand);

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
    // Syncing from the vehicle state supersedes any not-yet-sent user input.
    m_debounceTimer.stop();
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
    m_statusLabel->setProperty("state", enabled ? "ok" : "warn");
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
}

void ClimatePage::updateReadouts()
{
    m_fanValue->setText(QString::number(m_fanSlider->value()));
    m_fanBar->setValue(m_fanSlider->value());
}

void ClimatePage::onControlChanged()
{
    if (m_syncing || m_pendingRequestId)
        return;

    updateReadouts();
    m_debounceTimer.start(); // restart the debounce window, do not send yet
}

void ClimatePage::issueClimateCommand()
{
    if (m_pendingRequestId)
        return;

    m_lastSent = readControls();
    const quint32 requestId = m_car->sendClimateCommand(m_lastSent);
    if (!requestId) {
        m_cancelButton->setEnabled(false);
        setPanelEnabled(true, QStringLiteral("发送失败，已回滚"));
        applyControls(m_car->climate());
        return;
    }

    m_pendingRequestId = requestId;
    m_cancelButton->setEnabled(true);
    setPanelEnabled(false, QStringLiteral("等待车辆确认..."));
}

void ClimatePage::onClimateStateChanged(const sc::ClimateState &state)
{
    if (!m_pendingRequestId)
        applyControls(state);
}

void ClimatePage::onAckOk(quint32 requestId)
{
    if (!m_pendingRequestId || requestId != m_pendingRequestId)
        return;

    m_pendingRequestId = 0;
    m_cancelButton->setEnabled(false);
    setPanelEnabled(true, QStringLiteral("已确认"));
    applyControls(m_car->climate());
}

void ClimatePage::onAckTimeout(quint32 requestId)
{
    if (!m_pendingRequestId || requestId != m_pendingRequestId)
        return;

    m_pendingRequestId = 0;
    m_cancelButton->setEnabled(false);
    setPanelEnabled(true, QStringLiteral("超时未确认，已回滚"));
    applyControls(m_car->climate());
}

void ClimatePage::onCommandCancelled(quint32 requestId)
{
    if (!m_pendingRequestId || requestId != m_pendingRequestId)
        return;

    m_pendingRequestId = 0;
    m_cancelButton->setEnabled(false);
    setPanelEnabled(true, QStringLiteral("已取消"));
    applyControls(m_car->climate());
}

} // namespace sc
