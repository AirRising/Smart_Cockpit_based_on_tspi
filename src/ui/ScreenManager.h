#pragma once

#include <QHash>
#include <QObject>

#include "SystemState.h"

class QStackedWidget;
class QWidget;

namespace sc {

// Priority-based page manager on top of a QStackedWidget.
//
// - Normal pages (instrument/media/climate) can only replace a page with
//   equal or lower priority.
// - The reverse camera requests Urgent priority and therefore always steals
//   focus; dismissing it returns to the highest-priority remaining page.
class ScreenManager : public QObject
{
    Q_OBJECT
public:
    explicit ScreenManager(QStackedWidget *stack, QObject *parent = nullptr);

    void registerPage(sc::PageId id, QWidget *page);

    // Returns false when a higher-priority page currently owns the screen.
    bool requestPage(sc::PageId id, sc::PagePriority priority = sc::PagePriority::Normal);
    void dismissPage(sc::PageId id);

    sc::PageId currentPage() const;
    sc::PageId previousPage() const;

signals:
    void pageChanged(sc::PageId page);

private:
    void switchTo(sc::PageId id);
    sc::PageId highestPriorityActive() const;

    QStackedWidget *m_stack = nullptr;
    QHash<sc::PageId, int> m_pageIndex;
    QHash<sc::PageId, sc::PagePriority> m_activePriorities;
    sc::PageId m_current = sc::PageId::Instrument;
    sc::PageId m_previous = sc::PageId::Instrument;
};

} // namespace sc
