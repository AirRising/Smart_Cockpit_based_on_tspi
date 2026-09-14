// Pure C++ core test: this executable links ONLY the Qt-free smart_cockpit_cxx
// target. If any Qt header ever leaks into the core modules this test will no
// longer build, which is exactly the guard rail we want.

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "AsyncLoggerCore.h"
#include "CanFrameParserCore.h"
#include "CanTypes.h"

namespace {

int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": "      \
                      << #cond << std::endl;                                 \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

#define CHECK_NEAR(actual, expected)                                         \
    do {                                                                     \
        const double _a = (actual);                                          \
        const double _b = (expected);                                        \
        if (std::fabs(_a - _b) > 1e-9) {                                     \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": "      \
                      << #actual << " == " << _a << " (expected " << _b      \
                      << ")" << std::endl;                                   \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

std::vector<std::uint8_t> fromHex(const std::string &hex)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
        bytes.push_back(static_cast<std::uint8_t>(
            std::stoi(hex.substr(i, 2), nullptr, 16)));
    return bytes;
}

void decodeBuiltinFrame100()
{
    sc::CanFrameParserCore parser;
    std::unordered_map<std::string, double> values;
    // speed=120, rpm=3000, fuel raw=100 -> 50%, left_indicator bit0 of byte5.
    CHECK(parser.decode(sc::FrameId::ICU_Dynamic,
                        fromHex("7800b80b64010000"), &values));
    CHECK_NEAR(values["speed"], 120.0);
    CHECK_NEAR(values["engine_rpm"], 3000.0);
    CHECK_NEAR(values["fuel_level"], 50.0);
    CHECK(values["left_indicator"] > 0.5);
    CHECK(values["right_indicator"] < 0.5);
    CHECK(values["mil_engine"] < 0.5);
}

void truncatedFrameRejected()
{
    sc::CanFrameParserCore parser;
    std::unordered_map<std::string, double> values;
    CHECK(!parser.decode(sc::FrameId::ICU_Dynamic,
                         std::vector<std::uint8_t>(4, 0), &values));
    CHECK(parser.decode(sc::FrameId::ICU_Dynamic,
                        std::vector<std::uint8_t>(8, 0), &values));
    CHECK(values.count("speed") == 1);
}

void dbcTextOverridesBuiltins()
{
    sc::CanFrameParserCore parser;
    const std::string dbc =
        "VERSION \"demo\"\n"
        "BO_ 256 ICU: 8 ICU\n"
        " SG_ speed : 0|16@1+ (1,0) [0|240] \"km/h\" ICU\n"
        " SG_ engine_rpm : 16|16@1+ (1,0) [0|8000] \"rpm\" ICU\n"
        "BO_ 512 HVAC: 8 HVAC\n"
        " SG_ temp_set : 0|8@1+ (1,0) [16|30] \"C\" HVAC\n";
    CHECK(parser.loadDbcText(dbc));

    std::unordered_map<std::string, double> values;
    CHECK(parser.decode(256, fromHex("7800000000000000"), &values));
    CHECK_NEAR(values["speed"], 120.0);
    CHECK(parser.decode(512, fromHex("1800000000000000"), &values));
    CHECK_NEAR(values["temp_set"], 24.0);
}

void encodeDecodeRoundTrip200()
{
    sc::CanFrameParserCore parser;
    std::unordered_map<std::string, double> values = {
        { "temp_set", 24.0 },
        { "fan_speed", 3.0 },
        { "ac_on", 1.0 },
    };

    std::vector<std::uint8_t> payload;
    CHECK(parser.encode(sc::FrameId::HVAC, values, &payload));

    std::unordered_map<std::string, double> decoded;
    CHECK(parser.decode(sc::FrameId::HVAC, payload, &decoded));
    CHECK_NEAR(decoded["temp_set"], 24.0);
    CHECK_NEAR(decoded["fan_speed"], 3.0);
    CHECK(decoded["ac_on"] > 0.5);
}

void loggerWritesFile()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path dir = fs::temp_directory_path(ec) / "smart-cockpit-core-test";
    fs::remove_all(dir, ec);

    sc::AsyncLoggerCore::instance().start(dir.string(), sc::AsyncLoggerCore::Info);
    sc::AsyncLoggerCore::instance().info("hello core logger");
    sc::AsyncLoggerCore::instance().stop();

    const fs::path file = dir / "smart-cockpit.log";
    CHECK(fs::exists(file));
    std::ifstream in(file);
    std::stringstream content;
    content << in.rdbuf();
    CHECK(content.str().find("hello core logger") != std::string::npos);

    fs::remove_all(dir, ec);
}

} // namespace

int main()
{
    decodeBuiltinFrame100();
    truncatedFrameRejected();
    dbcTextOverridesBuiltins();
    encodeDecodeRoundTrip200();
    loggerWritesFile();

    if (g_failures != 0) {
        std::cerr << g_failures << " core check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "all core (Qt-free) checks passed" << std::endl;
    return 0;
}
