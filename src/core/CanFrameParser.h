#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

#include <vector>

#include "CanFrameParserCore.h"
#include "CanTypes.h"

namespace sc {

// Thin Qt adapter over the pure C++ CanFrameParserCore.
//
// The parsing/encoding logic (and the DBC subset reader) lives in
// CanFrameParserCore and pulls in no Qt headers; this class only converts
// between Qt containers and the std:: types at the boundary.
class CanFrameParser
{
public:
    CanFrameParser();

    bool loadDbcFile(const QString &path);
    bool loadDbcText(const QString &text);

    bool decode(quint32 id, const QByteArray &data, QHash<QString, double> *out) const;
    bool encode(quint32 id, const QHash<QString, double> &values, QByteArray *out) const;

    std::vector<CanSignalDef> signalDefs(quint32 id) const;
    QString frameName(quint32 id) const;

    // Escape hatch for code that wants the Qt-free core directly.
    CanFrameParserCore &core() { return m_core; }
    const CanFrameParserCore &core() const { return m_core; }

private:
    CanFrameParserCore m_core;
};

} // namespace sc
