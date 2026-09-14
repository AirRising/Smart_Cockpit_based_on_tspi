#pragma once

#include <cstdint>
#include <string>

// Pure C++ (standard library only) CAN data types. This header must stay free
// of Qt so the parsing core can be built and unit-tested without Qt.

namespace sc {

// One DBC signal definition.
struct CanSignalDef {
    std::string name;
    int startBit = 0;       // DBC start bit (Intel: LSB index, Motorola: MSB index)
    int bitLength = 1;
    bool bigEndian = false; // true = Motorola (SAE J1939 style), false = Intel
    bool isSigned = false;
    double factor = 1.0;
    double offset = 0.0;
    double minVal = 0.0;
    double maxVal = 0.0;
    std::string unit;
};

// Standard frame ids used by the cluster.
namespace FrameId {
constexpr std::uint32_t ICU_Dynamic   = 0x100; // speed/rpm/fuel/indicators/mil
constexpr std::uint32_t HVAC          = 0x200; // climate command + echo/ack
constexpr std::uint32_t Gear          = 0x300; // gear_signal (bit0 = R engaged)
constexpr std::uint32_t Chassis       = 0x400; // steering angle
} // namespace FrameId

} // namespace sc
