#include "CanFrameParserCore.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace sc {

namespace {

std::string trim(const std::string &s)
{
    const std::size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return {};
    const std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

bool startsWith(const std::string &s, const char *prefix)
{
    return s.rfind(prefix, 0) == 0;
}

std::vector<std::string> tokenize(const std::string &s)
{
    std::vector<std::string> tokens;
    std::istringstream stream(s);
    std::string token;
    while (stream >> token)
        tokens.push_back(token);
    return tokens;
}

bool parseUint(const std::string &s, std::uint32_t *out)
{
    try {
        std::size_t pos = 0;
        const unsigned long value = std::stoul(s, &pos, 10);
        if (pos != s.size())
            return false;
        *out = static_cast<std::uint32_t>(value);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseInt(const std::string &s, int *out)
{
    try {
        std::size_t pos = 0;
        const int value = std::stoi(s, &pos, 10);
        if (pos != s.size())
            return false;
        *out = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool parseDouble(const std::string &s, double *out)
{
    try {
        std::size_t pos = 0;
        const double value = std::stod(s, &pos);
        if (pos != s.size())
            return false;
        *out = value;
        return true;
    } catch (...) {
        return false;
    }
}

void stripChar(std::string *s, char c)
{
    s->erase(std::remove(s->begin(), s->end(), c), s->end());
}

} // namespace

CanFrameParserCore::CanFrameParserCore()
{
    installBuiltinTable();
}

void CanFrameParserCore::installBuiltinTable()
{
    // 0x100 ICU: instrument cluster dynamic data (CAN -> UI).
    m_frames[FrameId::ICU_Dynamic] = {
        { "speed",            0, 16, false, false, 1.0, 0.0, 0, 240, "km/h" },
        { "engine_rpm",      16, 16, false, false, 1.0, 0.0, 0, 8000, "rpm" },
        { "fuel_level",      32,  8, false, false, 0.5, 0.0, 0, 100, "%" },
        { "left_indicator",  40,  1, false, false, 1.0, 0.0, 0, 1, "" },
        { "right_indicator", 41,  1, false, false, 1.0, 0.0, 0, 1, "" },
        { "hazard",          42,  1, false, false, 1.0, 0.0, 0, 1, "" },
        { "mil_engine",      48,  1, false, false, 1.0, 0.0, 0, 1, "" },
        { "mil_abs",         49,  1, false, false, 1.0, 0.0, 0, 1, "" },
        { "mil_airbag",      50,  1, false, false, 1.0, 0.0, 0, 1, "" },
    };

    // 0x200 HVAC: command (UI -> CAN) and echo + ack bit (CAN -> UI).
    m_frames[FrameId::HVAC] = {
        { "temp_set",   0,  8, false, false, 1.0, 0.0, 16, 30, "C" },
        { "fan_speed",  8,  4, false, false, 1.0, 0.0, 0, 7, "" },
        { "blow_mode", 12,  2, false, false, 1.0, 0.0, 0, 3, "" },
        { "ac_on",     14,  1, false, false, 1.0, 0.0, 0, 1, "" },
        { "auto_mode", 15,  1, false, false, 1.0, 0.0, 0, 1, "" },
        { "ack",       16,  1, false, false, 1.0, 0.0, 0, 1, "" },
    };

    // 0x300 Gear: byte0 bit0 = reverse engaged (per requirement), bits 1..3 = PRND.
    m_frames[FrameId::Gear] = {
        { "gear_signal", 0, 8, false, false, 1.0, 0.0, 0, 7, "" },
    };

    // 0x400 Chassis: steering angle for reverse trajectory overlay.
    m_frames[FrameId::Chassis] = {
        { "steering_angle", 0, 16, false, true, 0.1, 0.0, -900, 900, "deg" },
    };
}

bool CanFrameParserCore::loadDbcFile(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return loadDbcText(buffer.str());
}

bool CanFrameParserCore::loadDbcText(const std::string &text)
{
    std::uint32_t currentFrameId = 0;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!parseDbcLine(line, &currentFrameId))
            return false;
    }
    return true;
}

bool CanFrameParserCore::parseDbcLine(const std::string &line,
                                      std::uint32_t *currentFrameId)
{
    const std::string trimmed = trim(line);
    if (trimmed.empty() || startsWith(trimmed, "VERSION") ||
        startsWith(trimmed, "NS_") || startsWith(trimmed, "BS_") ||
        startsWith(trimmed, "CM_") || startsWith(trimmed, "VAL_"))
        return true;

    const std::vector<std::string> tokens = tokenize(trimmed);
    if (tokens.empty())
        return true;

    if (tokens[0] == "BO_") {
        // BO_ <id> <name>: <dlc> <node>
        if (tokens.size() < 3)
            return false;
        std::uint32_t id = 0;
        if (!parseUint(tokens[1], &id))
            return false;
        // A loaded DBC fully overrides the built-in table for this frame id.
        *currentFrameId = id;
        m_frames[id] = {};
        return true;
    }

    if (tokens[0] == "SG_") {
        // SG_ <name> : <start>|<len>@<order><sign> (<factor>,<offset>) [<min>|<max>] "<unit>" <node>
        //   tokens: [0]SG_ [1]name [2]: [3]layout [4](f,o) [5][min|max] [6]"unit" [7]node
        if (tokens.size() < 5)
            return false;

        CanSignalDef def;
        def.name = tokens[1];

        const std::string &layout = tokens[3]; // e.g. "0|16@1+"
        const std::size_t bar = layout.find('|');
        if (bar == std::string::npos)
            return false;
        int startBit = 0;
        if (!parseInt(layout.substr(0, bar), &startBit))
            return false;
        def.startBit = startBit;

        const std::string lenOrder = layout.substr(bar + 1); // e.g. "16@1+"
        const std::size_t at = lenOrder.find('@');
        if (at == std::string::npos)
            return false;
        int bitLength = 0;
        if (!parseInt(lenOrder.substr(0, at), &bitLength))
            return false;
        def.bitLength = bitLength;
        def.bigEndian = lenOrder.substr(at + 1, 1) == "0";
        def.isSigned = !lenOrder.empty() && lenOrder.back() == '-';

        // "(factor,offset)"
        std::string factorOffset = tokens[4];
        stripChar(&factorOffset, '(');
        stripChar(&factorOffset, ')');
        const std::size_t comma = factorOffset.find(',');
        if (comma == std::string::npos)
            return false;
        double factor = 0.0;
        double offset = 0.0;
        if (!parseDouble(factorOffset.substr(0, comma), &factor))
            return false;
        if (!parseDouble(factorOffset.substr(comma + 1), &offset))
            return false;
        def.factor = factor;
        def.offset = offset;

        // "[min|max]" and unit are optional in our subset.
        if (tokens.size() >= 6 && !tokens[5].empty() && tokens[5][0] == '[') {
            std::string range = tokens[5];
            stripChar(&range, '[');
            stripChar(&range, ']');
            const std::size_t pipe = range.find('|');
            if (pipe != std::string::npos) {
                double minVal = 0.0;
                double maxVal = 0.0;
                if (parseDouble(range.substr(0, pipe), &minVal) &&
                    parseDouble(range.substr(pipe + 1), &maxVal)) {
                    def.minVal = minVal;
                    def.maxVal = maxVal;
                }
            }
        }
        if (tokens.size() >= 7 && !tokens[6].empty() && tokens[6][0] == '"') {
            std::string unit = tokens[6];
            stripChar(&unit, '"');
            def.unit = unit;
        }

        m_frames[*currentFrameId].push_back(def);
        return true;
    }

    return true; // ignore anything else (attribute lines etc.)
}

double CanFrameParserCore::decodeSignal(const CanSignalDef &def,
                                        const std::vector<std::uint8_t> &data)
{
    if (def.startBit / 8 + (def.bitLength + 7) / 8 >
        static_cast<int>(data.size()))
        return 0.0;

    std::uint64_t raw = 0;
    if (def.bigEndian) {
        // Motorola: startBit points at the MSB of the signal. Bits run
        // right-to-left inside the first byte, then continue in the next byte.
        int byteIdx = def.startBit / 8;
        int bitInByte = 7 - (def.startBit % 8);
        for (int i = 0; i < def.bitLength; ++i) {
            raw = (raw << 1) |
                  static_cast<std::uint64_t>((data[byteIdx] >> bitInByte) & 0x01);
            --bitInByte;
            if (bitInByte < 0) {
                bitInByte = 7;
                ++byteIdx;
            }
        }
    } else {
        // Intel: startBit is the LSB; bits run left-to-right.
        int byteIdx = def.startBit / 8;
        int bitInByte = def.startBit % 8;
        for (int i = 0; i < def.bitLength; ++i) {
            raw |= static_cast<std::uint64_t>((data[byteIdx] >> bitInByte) & 0x01) << i;
            ++bitInByte;
            if (bitInByte == 8) {
                bitInByte = 0;
                ++byteIdx;
            }
        }
    }

    if (def.isSigned && def.bitLength < 64) {
        const std::uint64_t signBit = std::uint64_t(1) << (def.bitLength - 1);
        if (raw & signBit)
            raw |= ~((std::uint64_t(1) << def.bitLength) - 1);
    }

    return static_cast<double>(static_cast<std::int64_t>(raw)) * def.factor +
           def.offset;
}

bool CanFrameParserCore::encodeSignal(const CanSignalDef &def, double value,
                                      std::vector<std::uint8_t> *out)
{
    if (static_cast<int>(out->size()) <
        def.startBit / 8 + (def.bitLength + 7) / 8)
        out->resize(def.startBit / 8 + (def.bitLength + 7) / 8);

    std::int64_t raw = std::llround((value - def.offset) / def.factor);
    if (def.bitLength < 64)
        raw &= (std::int64_t(1) << def.bitLength) - 1;

    if (def.bigEndian) {
        int byteIdx = def.startBit / 8;
        int bitInByte = 7 - (def.startBit % 8);
        for (int i = def.bitLength - 1; i >= 0; --i) {
            const bool bit = (raw >> i) & 0x01;
            if (bit)
                (*out)[byteIdx] = static_cast<std::uint8_t>((*out)[byteIdx] | (1 << bitInByte));
            else
                (*out)[byteIdx] = static_cast<std::uint8_t>((*out)[byteIdx] & ~(1 << bitInByte));
            --bitInByte;
            if (bitInByte < 0) {
                bitInByte = 7;
                ++byteIdx;
            }
        }
    } else {
        int byteIdx = def.startBit / 8;
        int bitInByte = def.startBit % 8;
        for (int i = 0; i < def.bitLength; ++i) {
            const bool bit = (raw >> i) & 0x01;
            if (bit)
                (*out)[byteIdx] = static_cast<std::uint8_t>((*out)[byteIdx] | (1 << bitInByte));
            else
                (*out)[byteIdx] = static_cast<std::uint8_t>((*out)[byteIdx] & ~(1 << bitInByte));
            ++bitInByte;
            if (bitInByte == 8) {
                bitInByte = 0;
                ++byteIdx;
            }
        }
    }
    return true;
}

bool CanFrameParserCore::decode(
    std::uint32_t id, const std::vector<std::uint8_t> &data,
    std::unordered_map<std::string, double> *out) const
{
    const auto it = m_frames.find(id);
    if (it == m_frames.end())
        return false;

    // All-or-nothing: a truncated/partial frame must not be decoded into a
    // mix of real values and zeroed fields. Reject it so callers keep the
    // last known-good state instead of showing misleading data.
    for (const CanSignalDef &def : it->second) {
        const int needBytes = def.startBit / 8 + (def.bitLength + 7) / 8;
        if (needBytes > static_cast<int>(data.size()))
            return false;
    }

    out->clear();
    for (const CanSignalDef &def : it->second)
        (*out)[def.name] = decodeSignal(def, data);
    return true;
}

bool CanFrameParserCore::encode(
    std::uint32_t id, const std::unordered_map<std::string, double> &values,
    std::vector<std::uint8_t> *out) const
{
    const auto it = m_frames.find(id);
    if (it == m_frames.end())
        return false;
    out->clear();
    out->resize(8);
    for (const CanSignalDef &def : it->second) {
        const auto vit = values.find(def.name);
        if (vit != values.end())
            encodeSignal(def, vit->second, out);
    }
    return true;
}

std::vector<CanSignalDef> CanFrameParserCore::signalDefs(std::uint32_t id) const
{
    const auto it = m_frames.find(id);
    return it == m_frames.end() ? std::vector<CanSignalDef>{} : it->second;
}

std::string CanFrameParserCore::frameName(std::uint32_t id) const
{
    switch (id) {
    case FrameId::ICU_Dynamic: return "ICU_Dynamic";
    case FrameId::HVAC:        return "HVAC";
    case FrameId::Gear:        return "Gear";
    case FrameId::Chassis:     return "Chassis";
    default: {
        std::ostringstream oss;
        oss << "Unknown_0x" << std::hex << id;
        return oss.str();
    }
    }
}

} // namespace sc
