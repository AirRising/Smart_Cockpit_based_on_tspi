#include "CanFrameParser.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <QtMath>

#include <cstring>

namespace sc {

CanFrameParser::CanFrameParser()
{
    installBuiltinTable();
}

void CanFrameParser::installBuiltinTable()
{
    // 0x100 ICU: instrument cluster dynamic data (CAN -> UI).
    m_frames.insert(FrameId::ICU_Dynamic, {
        { QStringLiteral("speed"),          0, 16, false, false, 1.0, 0.0, 0, 240, QStringLiteral("km/h") },
        { QStringLiteral("engine_rpm"),    16, 16, false, false, 1.0, 0.0, 0, 8000, QStringLiteral("rpm") },
        { QStringLiteral("fuel_level"),    32,  8, false, false, 0.5, 0.0, 0, 100, QStringLiteral("%") },
        { QStringLiteral("left_indicator"),40,  1, false, false, 1.0, 0.0, 0, 1, QString() },
        { QStringLiteral("right_indicator"),41, 1, false, false, 1.0, 0.0, 0, 1, QString() },
        { QStringLiteral("hazard"),        42,  1, false, false, 1.0, 0.0, 0, 1, QString() },
        { QStringLiteral("mil_engine"),    48,  1, false, false, 1.0, 0.0, 0, 1, QString() },
        { QStringLiteral("mil_abs"),       49,  1, false, false, 1.0, 0.0, 0, 1, QString() },
        { QStringLiteral("mil_airbag"),    50,  1, false, false, 1.0, 0.0, 0, 1, QString() },
    });

    // 0x200 HVAC: command (UI -> CAN) and echo + ack bit (CAN -> UI).
    m_frames.insert(FrameId::HVAC, {
        { QStringLiteral("temp_set"),   0,  8, false, false, 1.0, 0.0, 16, 30, QStringLiteral("C") },
        { QStringLiteral("fan_speed"),  8,  4, false, false, 1.0, 0.0, 0, 7, QString() },
        { QStringLiteral("blow_mode"), 12,  2, false, false, 1.0, 0.0, 0, 3, QString() },
        { QStringLiteral("ac_on"),     14,  1, false, false, 1.0, 0.0, 0, 1, QString() },
        { QStringLiteral("auto_mode"), 15,  1, false, false, 1.0, 0.0, 0, 1, QString() },
        { QStringLiteral("ack"),       16,  1, false, false, 1.0, 0.0, 0, 1, QString() },
    });

    // 0x300 Gear: byte0 bit0 = reverse engaged (per requirement), bits 1..3 = PRND.
    m_frames.insert(FrameId::Gear, {
        { QStringLiteral("gear_signal"), 0, 8, false, false, 1.0, 0.0, 0, 7, QString() },
    });

    // 0x400 Chassis: steering angle for reverse trajectory overlay.
    m_frames.insert(FrameId::Chassis, {
        { QStringLiteral("steering_angle"), 0, 16, false, true, 0.1, 0.0, -900, 900, QStringLiteral("deg") },
    });
}

bool CanFrameParser::loadDbcFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream stream(&file);
    const QString text = stream.readAll();
    file.close();
    return loadDbcText(text);
}

bool CanFrameParser::loadDbcText(const QString &text)
{
    quint32 currentFrameId = 0;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (!parseDbcLine(line, &currentFrameId))
            return false;
    }
    return true;
}

bool CanFrameParser::parseDbcLine(const QString &line, quint32 *currentFrameId)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1String("VERSION")) ||
        trimmed.startsWith(QLatin1String("NS_")) || trimmed.startsWith(QLatin1String("BS_")) ||
        trimmed.startsWith(QLatin1String("CM_")) || trimmed.startsWith(QLatin1String("VAL_")))
        return true;

    QStringList tokens;
    const QStringList parts = trimmed.split(
        QRegularExpression(QStringLiteral("\\s+")));
    for (const QString &part : parts) {
        if (!part.isEmpty())
            tokens.append(part);
    }
    if (tokens.isEmpty())
        return true;

    if (tokens.at(0) == QLatin1String("BO_")) {
        // BO_ <id> <name>: <dlc> <node>
        if (tokens.size() < 3)
            return false;
        bool ok = false;
        *currentFrameId = tokens.at(1).toUInt(&ok);
        if (!ok)
            return false;
        // A loaded DBC fully overrides the built-in table for this frame id.
        m_frames.remove(*currentFrameId);
        m_frames.insert(*currentFrameId, QList<CanSignalDef>());
        return true;
    }

    if (tokens.at(0) == QLatin1String("SG_")) {
        // SG_ <name> : <start>|<len>@<order><sign> (<factor>,<offset>) [<min>|<max>] "<unit>" <node>
        //   tokens: [0]SG_ [1]name [2]: [3]layout [4](f,o) [5][min|max] [6]"unit" [7]node
        CanSignalDef def;
        def.name = tokens.at(1);
        const QString layout = tokens.at(3); // e.g. "0|16@1+"
        const QStringList bits = layout.split(QLatin1Char('|'));
        if (bits.size() != 2)
            return false;
        bool ok = false;
        def.startBit = bits.at(0).toInt(&ok);
        if (!ok)
            return false;
        const QString lenOrder = bits.at(1); // e.g. "16@1+"
        const int at = lenOrder.indexOf(QLatin1Char('@'));
        if (at < 0)
            return false;
        def.bitLength = lenOrder.left(at).toInt(&ok);
        if (!ok)
            return false;
        def.bigEndian = lenOrder.mid(at + 1, 1) == QLatin1String("0");
        def.isSigned = lenOrder.endsWith(QLatin1Char('-'));

        // "(factor,offset)"
        if (tokens.size() < 5)
            return false;
        QString factorOffset = tokens.at(4);
        factorOffset.remove(QLatin1Char('(')).remove(QLatin1Char(')'));
        const QStringList fo = factorOffset.split(QLatin1Char(','));
        if (fo.size() != 2)
            return false;
        def.factor = fo.at(0).toDouble(&ok);
        if (!ok)
            return false;
        def.offset = fo.at(1).toDouble(&ok);
        if (!ok)
            return false;

        // "[min|max]" and unit are optional in our subset.
        if (tokens.size() >= 6 && tokens.at(5).startsWith(QLatin1Char('['))) {
            QString range = tokens.at(5);
            range.remove(QLatin1Char('[')).remove(QLatin1Char(']'));
            const QStringList minMax = range.split(QLatin1Char('|'));
            if (minMax.size() == 2) {
                def.minVal = minMax.at(0).toDouble();
                def.maxVal = minMax.at(1).toDouble();
            }
        }
        if (tokens.size() >= 7 && tokens.at(6).startsWith(QLatin1Char('"'))) {
            def.unit = tokens.at(6);
            def.unit.remove(QLatin1Char('"'));
        }

        m_frames[*currentFrameId].append(def);
        return true;
    }

    return true; // ignore anything else (attribute lines etc.)
}

double CanFrameParser::decodeSignal(const CanSignalDef &def, const QByteArray &data)
{
    if (def.startBit / 8 + (def.bitLength + 7) / 8 > data.size())
        return 0.0;

    quint64 raw = 0;
    if (def.bigEndian) {
        // Motorola: startBit points at the MSB of the signal. Bits run
        // right-to-left inside the first byte, then continue in the next byte.
        int byteIdx = def.startBit / 8;
        int bitInByte = 7 - (def.startBit % 8);
        for (int i = 0; i < def.bitLength; ++i) {
            raw = (raw << 1) | static_cast<quint64>((data.at(byteIdx) >> bitInByte) & 0x01);
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
            raw |= static_cast<quint64>((data.at(byteIdx) >> bitInByte) & 0x01) << i;
            ++bitInByte;
            if (bitInByte == 8) {
                bitInByte = 0;
                ++byteIdx;
            }
        }
    }

    if (def.isSigned && def.bitLength < 64) {
        const quint64 signBit = quint64(1) << (def.bitLength - 1);
        if (raw & signBit)
            raw |= ~((quint64(1) << def.bitLength) - 1);
    }

    return static_cast<double>(static_cast<qint64>(raw)) * def.factor + def.offset;
}

bool CanFrameParser::encodeSignal(const CanSignalDef &def, double value, QByteArray *out)
{
    if (out->size() < def.startBit / 8 + (def.bitLength + 7) / 8)
        out->resize(def.startBit / 8 + (def.bitLength + 7) / 8);

    qint64 raw = qRound64((value - def.offset) / def.factor);
    if (def.bitLength < 64)
        raw &= (qint64(1) << def.bitLength) - 1;

    if (def.bigEndian) {
        int byteIdx = def.startBit / 8;
        int bitInByte = 7 - (def.startBit % 8);
        for (int i = def.bitLength - 1; i >= 0; --i) {
            const bool bit = (raw >> i) & 0x01;
            if (bit)
                (*out)[byteIdx] = static_cast<char>((*out)[byteIdx] | (1 << bitInByte));
            else
                (*out)[byteIdx] = static_cast<char>((*out)[byteIdx] & ~(1 << bitInByte));
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
                (*out)[byteIdx] = static_cast<char>((*out)[byteIdx] | (1 << bitInByte));
            else
                (*out)[byteIdx] = static_cast<char>((*out)[byteIdx] & ~(1 << bitInByte));
            ++bitInByte;
            if (bitInByte == 8) {
                bitInByte = 0;
                ++byteIdx;
            }
        }
    }
    return true;
}

bool CanFrameParser::decode(quint32 id, const QByteArray &data, QHash<QString, double> *out) const
{
    const auto it = m_frames.constFind(id);
    if (it == m_frames.constEnd())
        return false;

    // All-or-nothing: a truncated/partial frame must not be decoded into a
    // mix of real values and zeroed fields. Reject it so callers keep the
    // last known-good state instead of showing misleading data.
    for (const CanSignalDef &def : it.value()) {
        const int needBytes = def.startBit / 8 + (def.bitLength + 7) / 8;
        if (needBytes > data.size())
            return false;
    }

    out->clear();
    for (const CanSignalDef &def : it.value())
        out->insert(def.name, decodeSignal(def, data));
    return true;
}

bool CanFrameParser::encode(quint32 id, const QHash<QString, double> &values, QByteArray *out) const
{
    const auto it = m_frames.constFind(id);
    if (it == m_frames.constEnd())
        return false;
    out->clear();
    out->resize(8);
    for (const CanSignalDef &def : it.value()) {
        const auto vit = values.constFind(def.name);
        if (vit != values.constEnd())
            encodeSignal(def, vit.value(), out);
    }
    return true;
}

QList<CanSignalDef> CanFrameParser::signalDefs(quint32 id) const
{
    return m_frames.value(id);
}

QString CanFrameParser::frameName(quint32 id) const
{
    switch (id) {
    case FrameId::ICU_Dynamic: return QStringLiteral("ICU_Dynamic");
    case FrameId::HVAC:        return QStringLiteral("HVAC");
    case FrameId::Gear:        return QStringLiteral("Gear");
    case FrameId::Chassis:     return QStringLiteral("Chassis");
    default:                   return QStringLiteral("Unknown_0x%1").arg(id, 0, 16);
    }
}

} // namespace sc
