#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "CanTypes.h"

// Pure C++ (standard library only) DBC reader/decoder. No Qt headers here: the
// Qt-facing API lives in CanFrameParser.h, which wraps this class.

namespace sc {

class CanFrameParserCore
{
public:
    CanFrameParserCore();

    bool loadDbcFile(const std::string &path);
    bool loadDbcText(const std::string &text);

    // Decode every known signal of `id` into `out`. Returns true when the
    // frame id is known and data is long enough.
    bool decode(std::uint32_t id, const std::vector<std::uint8_t> &data,
                std::unordered_map<std::string, double> *out) const;

    // Encode the given signal values into a CAN payload (Intel/Motorola aware).
    bool encode(std::uint32_t id,
                const std::unordered_map<std::string, double> &values,
                std::vector<std::uint8_t> *out) const;

    std::vector<CanSignalDef> signalDefs(std::uint32_t id) const;
    std::string frameName(std::uint32_t id) const;

    // Single-signal decode helpers (useful for diagnostics/tests).
    static double decodeSignal(const CanSignalDef &def,
                               const std::vector<std::uint8_t> &data);
    static bool encodeSignal(const CanSignalDef &def, double value,
                             std::vector<std::uint8_t> *out);

private:
    void installBuiltinTable();
    bool parseDbcLine(const std::string &line, std::uint32_t *currentFrameId);

    std::unordered_map<std::uint32_t, std::vector<CanSignalDef>> m_frames;
};

} // namespace sc
