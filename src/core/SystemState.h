#pragma once

#include <QMetaType>

namespace sc {

// System health state machine. HealthMonitor drives it and the UI banner
// reflects it (Normal green / Degraded orange / Emergency red).
enum class SystemState {
    Normal,    // all critical services healthy
    Degraded,  // non-critical service (camera/media/bluetooth) failed
    Emergency  // CAN bus lost -> cluster cannot function
};

// Pages managed by ScreenManager inside the QStackedWidget.
enum class PageId {
    Instrument = 0,
    Media,
    Climate,
    ReverseCamera
};

// Preemption priority. ReverseCamera is Urgent and always steals focus.
enum class PagePriority {
    Low = 0,
    Normal,
    High,
    Urgent
};

// Canonical climate state (confirmed state lives in CarService; the UI is
// only a view of it and rolls back on ack timeout).
struct ClimateState {
    int temperature = 22;   // 16..30 deg C
    int fanSpeed = 3;       // 0..7
    int blowMode = 0;       // 0=Face 1=Feet 2=Defrost 3=Face+Feet
    bool acOn = true;
    bool autoMode = false;

    bool operator==(const ClimateState &o) const
    {
        return temperature == o.temperature && fanSpeed == o.fanSpeed &&
               blowMode == o.blowMode && acOn == o.acOn && autoMode == o.autoMode;
    }
    bool operator!=(const ClimateState &o) const { return !(*this == o); }
};

} // namespace sc

Q_DECLARE_METATYPE(sc::SystemState)
Q_DECLARE_METATYPE(sc::PageId)
Q_DECLARE_METATYPE(sc::ClimateState)

namespace sc {

inline uint qHash(sc::PageId key, uint seed) { return ::qHash(static_cast<int>(key), seed); }
inline uint qHash(sc::PagePriority key, uint seed) { return ::qHash(static_cast<int>(key), seed); }

} // namespace sc
