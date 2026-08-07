#include "MainWindow.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "CameraService.h"
#include "CarService.h"
#include "ClimatePage.h"
#include "HealthMonitor.h"
#include "InstrumentPage.h"
#include "MediaPage.h"
#include "MediaService.h"
#include "ReverseCameraPage.h"
#include "ScreenManager.h"
#include "Theme.h"

namespace sc {

MainWindow::MainWindow(CarService *car, MediaService *media, CameraService *camera,
                       QWidget *parent)
    : QMainWindow(parent)
    , m_car(car)
{
    setWindowTitle(QStringLiteral("智能座舱"));

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    buildTopBar();
    layout->addWidget(m_topBar);

    m_stack = new QStackedWidget(this);
    m_screen = new ScreenManager(m_stack, this);

    auto *instrument = new InstrumentPage(car, this);
    auto *mediaPage = new MediaPage(media, this);
    auto *climate = new ClimatePage(car, this);
    auto *reverse = new ReverseCameraPage(camera, car, this);

    m_screen->registerPage(sc::PageId::Instrument, instrument);
    m_screen->registerPage(sc::PageId::Media, mediaPage);
    m_screen->registerPage(sc::PageId::Climate, climate);
    m_screen->registerPage(sc::PageId::ReverseCamera, reverse);

    layout->addWidget(m_stack, 1);

    buildNavBar();
    layout->addWidget(m_navBar);

    m_statusStrip = new QLabel(QStringLiteral("就绪"), this);
    m_statusStrip->setObjectName(QStringLiteral("statusStrip"));
    layout->addWidget(m_statusStrip);

    setCentralWidget(central);

    connect(car, &CarService::reverseChanged, this, &MainWindow::onReverseChanged);
    connect(car, &CarService::canConnectionChanged, this, [this](bool connected) {
        m_canStatus->setText(connected ? QStringLiteral("CAN \u25CF")
                                       : QStringLiteral("CAN \u25CB"));
        m_canStatus->setStyleSheet(connected
            ? QStringLiteral("background:#163B26; color:#7FE6A0; border:1px solid #2FBF71;"
                             "border-radius:10px; padding:4px 10px; font-weight:700;")
            : QStringLiteral("background:#1A2028; color:#6F7A90; border:1px solid #2A3550;"
                             "border-radius:10px; padding:4px 10px; font-weight:700;"));
    });

    m_clockTimer = new QTimer(this);
    m_clockTimer->setInterval(1000);
    connect(m_clockTimer, &QTimer::timeout, this, &MainWindow::updateClock);
    m_clockTimer->start();
    updateClock();

    onSystemStateChanged(sc::SystemState::Normal);

    buildShortcuts();
}

void MainWindow::buildTopBar()
{
    m_topBar = new QWidget(this);
    m_topBar->setObjectName(QStringLiteral("topBar"));
    m_topBar->setFixedHeight(60);

    auto *logo = new QLabel(QStringLiteral("SC"), m_topBar);
    logo->setObjectName(QStringLiteral("brandLogo"));
    logo->setAlignment(Qt::AlignCenter);

    auto *brandName = new QLabel(QStringLiteral("智能座舱"), m_topBar);
    brandName->setObjectName(QStringLiteral("brandName"));
    auto *brandTag = new QLabel(QStringLiteral("v1.0 \u00B7 车载HMI"), m_topBar);
    brandTag->setObjectName(QStringLiteral("brandTag"));

    auto *brandCol = new QVBoxLayout;
    brandCol->setSpacing(0);
    brandCol->addWidget(brandName);
    brandCol->addWidget(brandTag);

    m_pageTitle = new QLabel(QStringLiteral("仪表盘"), m_topBar);
    m_pageTitle->setObjectName(QStringLiteral("pageTitle"));

    m_canStatus = new QLabel(QStringLiteral("CAN \u25CB"), m_topBar);
    m_canStatus->setObjectName(QStringLiteral("canStatus"));
    m_canStatus->setStyleSheet(
        QStringLiteral("background:#1A2028; color:#6F7A90; border:1px solid #2A3550;"
                       "border-radius:10px; padding:4px 10px; font-weight:700;"));

    m_statePill = new QLabel(QStringLiteral("系统正常"), m_topBar);
    m_statePill->setObjectName(QStringLiteral("statePill"));

    auto *clockCol = new QVBoxLayout;
    clockCol->setSpacing(0);
    m_clockLabel = new QLabel(QStringLiteral("--:--"), m_topBar);
    m_clockLabel->setObjectName(QStringLiteral("clockLabel"));
    m_clockLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_dateLabel = new QLabel(QString(), m_topBar);
    m_dateLabel->setObjectName(QStringLiteral("dateLabel"));
    m_dateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    clockCol->addWidget(m_clockLabel);
    clockCol->addWidget(m_dateLabel);

    auto *layout = new QHBoxLayout(m_topBar);
    layout->setContentsMargins(18, 8, 18, 8);
    layout->setSpacing(12);
    layout->addWidget(logo);
    layout->addLayout(brandCol);
    layout->addSpacing(18);
    layout->addWidget(m_pageTitle);
    layout->addStretch();
    layout->addWidget(m_canStatus);
    layout->addWidget(m_statePill);
    layout->addSpacing(8);
    layout->addLayout(clockCol);
}

void MainWindow::buildNavBar()
{
    m_navBar = new QWidget(this);
    m_navBar->setObjectName(QStringLiteral("navBar"));
    m_navBar->setFixedHeight(64);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);

    auto addNav = [this](const QString &text, sc::PageId page) {
        auto *button = new QToolButton(m_navBar);
        button->setText(text);
        button->setObjectName(QStringLiteral("navButton"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        m_navGroup->addButton(button, int(page));
        connect(button, &QToolButton::clicked, this, [this, page]() {
            m_screen->requestPage(page, sc::PagePriority::Normal);
        });
        return button;
    };

    auto *layout = new QHBoxLayout(m_navBar);
    layout->setContentsMargins(16, 6, 16, 6);
    layout->setSpacing(8);
    layout->addStretch();
    layout->addWidget(addNav(QStringLiteral("\u25C9  仪表"), sc::PageId::Instrument));
    layout->addWidget(addNav(QStringLiteral("\u25B6  媒体"), sc::PageId::Media));
    layout->addWidget(addNav(QStringLiteral("\u2744  空调"), sc::PageId::Climate));
    layout->addStretch();

    connect(m_screen, &ScreenManager::pageChanged, this, [this](sc::PageId page) {
        updatePageTitle(page);
        QAbstractButton *button = m_navGroup->button(int(page));
        if (button)
            button->setChecked(true);
    });
    if (auto *initial = m_navGroup->button(int(sc::PageId::Instrument)))
        initial->setChecked(true);
}

void MainWindow::onSystemStateChanged(sc::SystemState state)
{
    switch (state) {
    case sc::SystemState::Normal:
        m_statePill->setText(QStringLiteral("系统正常"));
        m_statePill->setStyleSheet(
            QStringLiteral("background:#163B26; color:#7FE6A0; border:1px solid #2FBF71;"
                           "border-radius:12px; padding:4px 14px; font-weight:700;"));
        break;
    case sc::SystemState::Degraded:
        m_statePill->setText(QStringLiteral("系统降级"));
        m_statePill->setStyleSheet(
            QStringLiteral("background:#3D2F12; color:#FFC24B; border:1px solid #FFB400;"
                           "border-radius:12px; padding:4px 14px; font-weight:700;"));
        break;
    case sc::SystemState::Emergency:
        m_statePill->setText(QStringLiteral("CAN 丢失"));
        m_statePill->setStyleSheet(
            QStringLiteral("background:#3A1A1E; color:#FF9B9C; border:1px solid #FF4D4F;"
                           "border-radius:12px; padding:4px 14px; font-weight:700;"));
        break;
    }
}

void MainWindow::onReverseChanged(bool active)
{
    if (active) {
        // Urgent priority: always steals the screen.
        m_screen->requestPage(sc::PageId::ReverseCamera, sc::PagePriority::Urgent);
    } else {
        m_screen->dismissPage(sc::PageId::ReverseCamera);
    }
}

void MainWindow::onDiagnostics(const QString &summary)
{
    m_statusStrip->setText(summary);
}

void MainWindow::updateClock()
{
    const QDateTime now = QDateTime::currentDateTime();
    m_clockLabel->setText(now.toString(QStringLiteral("HH:mm")));
    m_dateLabel->setText(now.toString(QStringLiteral("ddd, MMM d")));
}

void MainWindow::updatePageTitle(sc::PageId page)
{
    switch (page) {
    case sc::PageId::Instrument:    m_pageTitle->setText(QStringLiteral("仪表盘")); break;
    case sc::PageId::Media:         m_pageTitle->setText(QStringLiteral("多媒体")); break;
    case sc::PageId::Climate:       m_pageTitle->setText(QStringLiteral("空调控制")); break;
    case sc::PageId::ReverseCamera: m_pageTitle->setText(QStringLiteral("倒车影像")); break;
    }
}

void MainWindow::buildShortcuts()
{
    // Debug-only navigation shortcuts (remove for production).
    auto addShortcut = [this](QKeySequence sequence, sc::PageId page) {
        auto *shortcut = new QShortcut(sequence, this);
        connect(shortcut, &QShortcut::activated, this, [this, page]() {
            m_screen->requestPage(page, sc::PagePriority::Normal);
        });
    };
    addShortcut(QKeySequence(Qt::Key_F1), sc::PageId::Instrument);
    addShortcut(QKeySequence(Qt::Key_F2), sc::PageId::Media);
    addShortcut(QKeySequence(Qt::Key_F3), sc::PageId::Climate);
}

} // namespace sc
