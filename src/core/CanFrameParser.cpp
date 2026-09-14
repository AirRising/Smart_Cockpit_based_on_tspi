#include "CanFrameParser.h"

#include <cstring>
#include <unordered_map>

namespace sc {

CanFrameParser::CanFrameParser() = default;

bool CanFrameParser::loadDbcFile(const QString &path)
{
    return m_core.loadDbcFile(path.toStdString());
}

bool CanFrameParser::loadDbcText(const QString &text)
{
    return m_core.loadDbcText(text.toStdString());
}

bool CanFrameParser::decode(quint32 id, const QByteArray &data,
                            QHash<QString, double> *out) const
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(static_cast<std::size_t>(data.size()));
    for (const char c : data)
        bytes.push_back(static_cast<std::uint8_t>(c));

    std::unordered_map<std::string, double> values;
    if (!m_core.decode(id, bytes, &values))
        return false;

    out->clear();
    for (const auto &kv : values)
        out->insert(QString::fromStdString(kv.first), kv.second);
    return true;
}

bool CanFrameParser::encode(quint32 id, const QHash<QString, double> &values,
                            QByteArray *out) const
{
    std::unordered_map<std::string, double> raw;
    raw.reserve(static_cast<std::size_t>(values.size()));
    for (auto it = values.constBegin(); it != values.constEnd(); ++it)
        raw.emplace(it.key().toStdString(), it.value());

    std::vector<std::uint8_t> bytes;
    if (!m_core.encode(id, raw, &bytes))
        return false;

    out->resize(static_cast<int>(bytes.size()));
    if (!bytes.empty())
        std::memcpy(out->data(), bytes.data(), bytes.size());
    return true;
}

std::vector<CanSignalDef> CanFrameParser::signalDefs(quint32 id) const
{
    return m_core.signalDefs(id);
}

QString CanFrameParser::frameName(quint32 id) const
{
    return QString::fromStdString(m_core.frameName(id));
}

} // namespace sc
