#pragma once

#include <QTimer>
#include <QWidget>

#include "SystemState.h"

class QButtonGroup;
class QLabel;
class QProgressBar;
class QPushButton;
class QSlider;

namespace sc {

class CarService;

// HVAC panel.
//
// Every UI change issues a 0x200 command through CarService and locks the
// panel until the vehicle echoes it with the ack bit. If the ack times out
// the panel rolls back to the last vehicle-confirmed state.
class ClimatePage : public QWidget
{
    Q_OBJECT
public:
    explicit ClimatePage(CarService *car, QWidget *parent = nullptr);

private slots:
    void onControlChanged();
    void onClimateStateChanged(const sc::ClimateState &state);
    void onAckOk(quint32 requestId);
    void onAckTimeout(quint32 requestId);
    void onCommandCancelled(quint32 requestId);

private:
    sc::ClimateState readControls() const;
    void applyControls(const sc::ClimateState &state);
    void setPanelEnabled(bool enabled, const QString &statusText);
    void updateReadouts();
    // Fires when the 250 ms debounce window ends: reads the controls and
    // issues the climate command, then locks the panel until the ack.
    void issueClimateCommand();

    CarService *m_car = nullptr;
    QSlider *m_tempSlider = nullptr;
    QSlider *m_fanSlider = nullptr;
    QLabel *m_fanValue = nullptr;
    QProgressBar *m_fanBar = nullptr;
    QButtonGroup *m_modeGroup = nullptr;
    QButtonGroup *m_circGroup = nullptr;
    QPushButton *m_acToggle = nullptr;
    QPushButton *m_autoToggle = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTimer m_debounceTimer;
    bool m_syncing = false;
    quint32 m_pendingRequestId = 0;
    sc::ClimateState m_lastSent;
};

} // namespace sc
