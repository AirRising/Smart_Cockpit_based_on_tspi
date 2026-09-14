#include <QtTest>

#include <QStackedWidget>
#include <QWidget>

#include "ScreenManager.h"
#include "SystemState.h"

using namespace sc;

namespace {

void registerPages(ScreenManager &sm, QStackedWidget &stack)
{
    auto *instrument = new QWidget(&stack);
    sm.registerPage(PageId::Instrument, instrument);
    auto *media = new QWidget(&stack);
    sm.registerPage(PageId::Media, media);
    auto *climate = new QWidget(&stack);
    sm.registerPage(PageId::Climate, climate);
    auto *reverse = new QWidget(&stack);
    sm.registerPage(PageId::ReverseCamera, reverse);
}

} // namespace

// ScreenManager priority/preemption logic: the "reverse steals everything"
// rule that keeps a normal nav request from interrupting an active reverse
// camera view.
class TestScreenManager : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void navSwitchesNormally();
    void urgentPreemptsAndBlocksLower();
    void dismissReturnsToPrevious();
};

void TestScreenManager::initTestCase()
{
    qRegisterMetaType<sc::PageId>("sc::PageId");
    qRegisterMetaType<sc::PagePriority>("sc::PagePriority");
}

void TestScreenManager::navSwitchesNormally()
{
    QStackedWidget stack;
    ScreenManager sm(&stack);
    registerPages(sm, stack);

    QSignalSpy spy(&sm, &ScreenManager::pageChanged);
    QVERIFY(sm.requestPage(PageId::Media, PagePriority::Normal));
    QCOMPARE(sm.currentPage(), PageId::Media);
    QCOMPARE(spy.count(), 1);

    QVERIFY(sm.requestPage(PageId::Climate, PagePriority::Normal));
    QCOMPARE(sm.currentPage(), PageId::Climate);
    QCOMPARE(spy.count(), 2);
}

void TestScreenManager::urgentPreemptsAndBlocksLower()
{
    QStackedWidget stack;
    ScreenManager sm(&stack);
    registerPages(sm, stack);

    QVERIFY(sm.requestPage(PageId::Media, PagePriority::Normal));
    QVERIFY(sm.requestPage(PageId::ReverseCamera, PagePriority::Urgent));
    QCOMPARE(sm.currentPage(), PageId::ReverseCamera);

    // A normal nav request must be blocked while the urgent page is active.
    QVERIFY(!sm.requestPage(PageId::Climate, PagePriority::Normal));
    QCOMPARE(sm.currentPage(), PageId::ReverseCamera);
}

void TestScreenManager::dismissReturnsToPrevious()
{
    QStackedWidget stack;
    ScreenManager sm(&stack);
    registerPages(sm, stack);

    QVERIFY(sm.requestPage(PageId::Media, PagePriority::Normal));
    QVERIFY(sm.requestPage(PageId::ReverseCamera, PagePriority::Urgent));
    sm.dismissPage(PageId::ReverseCamera);
    QCOMPARE(sm.currentPage(), PageId::Media);
}

QTEST_MAIN(TestScreenManager)
#include "tst_screenmanager.moc"
