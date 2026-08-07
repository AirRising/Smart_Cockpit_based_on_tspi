#include <QtTest>

#include "CanFrameParser.h"

using namespace sc;

class TestCanFrameParser : public QObject
{
    Q_OBJECT
private slots:
    void decodeFrame100();
    void decodeFrame300Reverse();
    void truncatedFrameRejected();
    void loadDbcText();
    void encodeFrame200();
};

void TestCanFrameParser::decodeFrame100()
{
    CanFrameParser parser;
    // speed=120 (0x78 0x00), rpm=3000 (0xb8 0x0b), fuel raw=100 -> 50%,
    // left_indicator bit0 of byte5.
    const QByteArray data = QByteArray::fromHex("7800b80b64010000");

    QHash<QString, double> vals;
    QVERIFY(parser.decode(FrameId::ICU_Dynamic, data, &vals));
    QCOMPARE(vals.value(QStringLiteral("speed")), 120.0);
    QCOMPARE(vals.value(QStringLiteral("engine_rpm")), 3000.0);
    QCOMPARE(vals.value(QStringLiteral("fuel_level")), 50.0);
    QVERIFY(vals.value(QStringLiteral("left_indicator")) > 0.5);
    QVERIFY(vals.value(QStringLiteral("right_indicator")) < 0.5);
    QVERIFY(vals.value(QStringLiteral("mil_engine")) < 0.5);
}

void TestCanFrameParser::decodeFrame300Reverse()
{
    CanFrameParser parser;
    const QByteArray data = QByteArray::fromHex("0100000000000000");

    QHash<QString, double> vals;
    QVERIFY(parser.decode(FrameId::Gear, data, &vals));
    // gear_signal byte: bit0 = reverse engaged -> raw 1.
    QCOMPARE(vals.value(QStringLiteral("gear_signal")), 1.0);
}

void TestCanFrameParser::truncatedFrameRejected()
{
    CanFrameParser parser;
    // 0x100's last signal (mil_airbag at startBit 50) needs 7 bytes; a shorter
    // frame must be rejected wholesale rather than decoded into zeroed fields.
    const QByteArray shortFrame = QByteArray(4, '\0');
    QHash<QString, double> vals;
    QVERIFY(!parser.decode(FrameId::ICU_Dynamic, shortFrame, &vals));

    const QByteArray fullFrame = QByteArray(8, '\0');
    QVERIFY(parser.decode(FrameId::ICU_Dynamic, fullFrame, &vals));
    QVERIFY(vals.contains(QStringLiteral("speed")));
}

void TestCanFrameParser::loadDbcText()
{
    CanFrameParser parser;
    const QString dbc = QStringLiteral(
        "VERSION \"demo\"\n"
        "BO_ 256 ICU: 8 ICU\n"
        " SG_ speed : 0|16@1+ (1,0) [0|240] \"km/h\" ICU\n"
        " SG_ engine_rpm : 16|16@1+ (1,0) [0|8000] \"rpm\" ICU\n"
        "BO_ 512 HVAC: 8 HVAC\n"
        " SG_ temp_set : 0|8@1+ (1,0) [16|30] \"C\" HVAC\n");
    QVERIFY(parser.loadDbcText(dbc));

    QHash<QString, double> vals;
    QVERIFY(parser.decode(256, QByteArray::fromHex("7800000000000000"), &vals));
    QCOMPARE(vals.value(QStringLiteral("speed")), 120.0);

    QVERIFY(parser.decode(512, QByteArray::fromHex("1800000000000000"), &vals));
    QCOMPARE(vals.value(QStringLiteral("temp_set")), 24.0);
}

void TestCanFrameParser::encodeFrame200()
{
    CanFrameParser parser;
    QHash<QString, double> values;
    values.insert(QStringLiteral("temp_set"), 24.0);
    values.insert(QStringLiteral("fan_speed"), 3.0);
    values.insert(QStringLiteral("ac_on"), 1.0);

    QByteArray payload;
    QVERIFY(parser.encode(FrameId::HVAC, values, &payload));

    QHash<QString, double> decoded;
    QVERIFY(parser.decode(FrameId::HVAC, payload, &decoded));
    QCOMPARE(decoded.value(QStringLiteral("temp_set")), 24.0);
    QCOMPARE(decoded.value(QStringLiteral("fan_speed")), 3.0);
    QVERIFY(decoded.value(QStringLiteral("ac_on")) > 0.5);
}

QTEST_GUILESS_MAIN(TestCanFrameParser)
#include "tst_canframeparser.moc"
