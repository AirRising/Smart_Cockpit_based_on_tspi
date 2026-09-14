#include "ScreenManager.h"

#include <QDebug>
#include <QStackedWidget>
#include <QWidget>

namespace sc {

ScreenManager::ScreenManager(QStackedWidget *stack, QObject *parent)
    : QObject(parent)
    , m_stack(stack)
{
}

void ScreenManager::registerPage(sc::PageId id, QWidget *page)
{
    m_pageIndex.insert(id, m_stack->addWidget(page));
}

bool ScreenManager::requestPage(sc::PageId id, sc::PagePriority priority)
{
    const sc::PagePriority currentPriority = m_activePriorities.value(m_current, sc::PagePriority::Low);
    if (id == m_current) {
        if (priority > currentPriority)
            m_activePriorities.insert(id, priority);
        return true;
    }
    if (priority < currentPriority)
        return false; // blocked by an urgent page (reverse camera)

    m_previous = m_current;
    m_activePriorities.insert(id, priority);
    switchTo(id);
    return true;
}

void ScreenManager::dismissPage(sc::PageId id)
{
    if (!m_activePriorities.contains(id))
        return;
    m_activePriorities.remove(id);
    if (m_current != id)
        return;

    if (m_activePriorities.isEmpty())
        switchTo(m_previous);
    else
        switchTo(highestPriorityActive());
}

sc::PageId ScreenManager::currentPage() const
{
    return m_current;
}

sc::PageId ScreenManager::previousPage() const
{
    return m_previous;
}

void ScreenManager::switchTo(sc::PageId id)
{
    if (!m_pageIndex.contains(id)) {
        qWarning() << "ScreenManager: page not registered" << int(id);
        return;
    }
    m_current = id;
    m_stack->setCurrentIndex(m_pageIndex.value(id));
    emit pageChanged(m_current);
}

sc::PageId ScreenManager::highestPriorityActive() const
{
    sc::PageId best = sc::PageId::Instrument;
    sc::PagePriority bestPriority = sc::PagePriority::Low;
    for (auto it = m_activePriorities.constBegin(); it != m_activePriorities.constEnd(); ++it) {
        if (it.value() > bestPriority) {
            best = it.key();
            bestPriority = it.value();
        }
    }
    return best;
}

} // namespace sc
