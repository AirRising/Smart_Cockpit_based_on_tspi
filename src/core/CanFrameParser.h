#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>

#include "CanTypes.h"

namespace sc {

// Lightweight DBC reader + decoder for the frames used by the cluster.
//
// - Ships with a built-in signal table for 0x100/0x200/0x300/0x400 so the
//   project runs out of the box (also used by the unit tests).
// - Can load a real .dbc file (the BO_ / SG_ subset) via loadDbcFile().
class CanFrameParser
{
public:
    CanFrameParser();

    bool loadDbcFile(const QString &path);
    bool loadDbcText(const QString &text);

    // Decode every known signal of `id` into `out`. Returns true when the
    // frame id is known and data is long enough.
    bool decode(quint32 id, const QByteArray &data, QHash<QString, double> *out) const;

    // Encode the given signal values into a CAN payload (Intel/Motorola aware).
    bool encode(quint32 id, const QHash<QString, double> &values, QByteArray *out) const;

    QList<CanSignalDef> signalDefs(quint32 id) const;
    QString frameName(quint32 id) const;

    // Single-signal decode helpers (useful for diagnostics/tests).
    static double decodeSignal(const CanSignalDef &def, const QByteArray &data);
    static bool encodeSignal(const CanSignalDef &def, double value, QByteArray *out);

private:
    void installBuiltinTable();
    bool parseDbcLine(const QString &line, quint32 *currentFrameId);

    QHash<quint32, QList<CanSignalDef>> m_frames;
};

} // namespace sc
