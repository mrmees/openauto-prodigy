#include "WirelessAaConfig.hpp"

#include <QMetaType>

namespace oap::aa {

quint16 resolveWirelessAaTcpPort(const QVariant& value, bool* usedFallback)
{
    if (usedFallback)
        *usedFallback = false;

    if (!value.isValid() || value.isNull())
        return kDefaultWirelessAaTcpPort;

    bool numericType = false;
    switch (value.metaType().id()) {
    case QMetaType::Char:
    case QMetaType::SChar:
    case QMetaType::UChar:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        numericType = true;
        break;
    default:
        break;
    }

    bool ok = false;
    const qlonglong parsed = value.toLongLong(&ok);
    const bool textualType = value.metaType().id() == QMetaType::QString;
    const bool ephemeralInteger = numericType && ok && parsed == 0;
    const bool productionPort = (numericType || textualType)
        && ok && parsed >= 1 && parsed <= 65535;
    if (ephemeralInteger || productionPort)
        return static_cast<quint16>(parsed);

    if (usedFallback)
        *usedFallback = true;
    return kDefaultWirelessAaTcpPort;
}

} // namespace oap::aa
