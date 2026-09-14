#pragma once

#include <QLabel>
#include <QWidget>

namespace sc {

class CarService;
class GaugeWidget;

// Digital instrument cluster page: speed / rpm gauges, fuel level, gear
// indicator and warning lamps, all fed by CarService signals.
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
    static void setLamp(QLabel *lamp, bool on);

    CarService *m_car = nullptr;
    GaugeWidget *m_speedGauge = nullptr;
    GaugeWidget *m_rpmGauge = nullptr;
    GaugeWidget *m_fuelGauge = nullptr;
    QLabel *m_gearLabel = nullptr;
    QLabel *m_leftLamp = nullptr;
    QLabel *m_rightLamp = nullptr;
    QLabel *m_hazardLamp = nullptr;
    QLabel *m_engineLamp = nullptr;
    QLabel *m_absLamp = nullptr;
    QLabel *m_airbagLamp = nullptr;
};

} // namespace sc
