#pragma once

#include <QWidget>

class QLabel;

namespace sc {

class CarService;
class GaugeWidget;
class BatteryWidget;

// Digital instrument cluster page: speed / rpm gauges, fuel level, gear
// indicator and warning lamps, all fed by CarService signals.
//
// Vertical three-section layout for the 800x1280 portrait screen:
// speed (primary) on top, rpm below, then a status card with the gear
// readout, battery level and the warning-lamp grid.
class InstrumentPage : public QWidget
{
    Q_OBJECT
public:
    explicit InstrumentPage(CarService *car, QWidget *parent = nullptr);

private slots:
    void onSpeedChanged(double kmh);
    void onRpmChanged(int rpm);
    void onFuelChanged(double percent);
    void onIndicators(bool left, bool right, bool hazard);
    void onMils(bool engine, bool abs, bool airbag);
    void onGearChanged(int position);

private:
    CarService *m_car = nullptr;
    GaugeWidget *m_speedGauge = nullptr;
    GaugeWidget *m_rpmGauge = nullptr;
    BatteryWidget *m_battery = nullptr;
    QLabel *m_gearLabel = nullptr;
    // Lamp chips are defined in the .cpp (anonymous namespace); stored as
    // base-class pointers here and cast back inside the .cpp only.
    QWidget *m_leftLamp = nullptr;
    QWidget *m_rightLamp = nullptr;
    QWidget *m_hazardLamp = nullptr;
    QWidget *m_engineLamp = nullptr;
    QWidget *m_absLamp = nullptr;
    QWidget *m_airbagLamp = nullptr;
};

} // namespace sc
