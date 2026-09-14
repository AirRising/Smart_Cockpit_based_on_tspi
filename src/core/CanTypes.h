#pragma once

#include <QString>

namespace sc {

// One DBC signal definition.
struct CanSignalDef {
    QString name;
    int startBit = 0;       // DBC start bit (Intel: LSB index, Motorola: MSB index)
    int bitLength = 1;
    bool bigEndian = false; // true = Motorola (SAE J1939 style), false = Intel
    bool isSigned = false;
    double factor = 1.0;
    double offset = 0.0;
    double minVal = 0.0;
    double maxVal = 0.0;
    QString unit;
};

// Standard frame ids used by the cluster.
namespace FrameId {
constexpr quint32 ICU_Dynamic   = 0x100; // speed/rpm/fuel/indicators/mil
constexpr quint32 HVAC          = 0x200; // climate command + echo/ack
constexpr quint32 Gear          = 0x300; // gear_signal (bit0 = R engaged)
constexpr quint32 Chassis       = 0x400; // steering angle
} // namespace FrameId

} // namespace sc
