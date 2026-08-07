#pragma once

#include <QMainWindow>

#include "SystemState.h"

class QButtonGroup;
class QLabel;
class QStackedWidget;
class QTimer;
class QWidget;

namespace sc {

class CameraService;
class CarService;
class ClimatePage;
class HealthMonitor;
class InstrumentPage;
class MediaPage;
class MediaService;
class ReverseCameraPage;
class ScreenManager;

// Main window: brand/status top bar, QStackedWidget with the pages below,
// bottom navigation bar and a diagnostics strip. Managed by ScreenManager
// (reverse view preempts everything).
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(CarService *car, MediaService *media, CameraService *camera,
               QWidget *parent = nullptr);

    ScreenManager *screenManager() const { return m_screen; }

public slots:
    void onSystemStateChanged(sc::SystemState state);
    void onReverseChanged(bool active);
    void onDiagnostics(const QString &summary);

private:
    void buildTopBar();
    void buildNavBar();
    void buildShortcuts();
    void updateClock();
    void updatePageTitle(sc::PageId page);

    QWidget *m_topBar = nullptr;
    QWidget *m_navBar = nullptr;
    QButtonGroup *m_navGroup = nullptr;
    QLabel *m_pageTitle = nullptr;
    QLabel *m_statePill = nullptr;
    QLabel *m_canStatus = nullptr;
    QLabel *m_clockLabel = nullptr;
    QLabel *m_dateLabel = nullptr;
    QLabel *m_statusStrip = nullptr;
    QStackedWidget *m_stack = nullptr;
    ScreenManager *m_screen = nullptr;
    CarService *m_car = nullptr;
    QTimer *m_clockTimer = nullptr;
};

} // namespace sc
