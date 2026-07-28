#include "GalVersionPolicy.hpp"

#include "../YamlConfig.hpp"

#include <QDebug>

namespace oap::aa {

QString galVersionToString(oaa::ProtocolVersion version)
{
    return QStringLiteral("%1.%2").arg(version.major).arg(version.minor);
}

bool parseSupportedGalVersion(const QString& text,
                              oaa::ProtocolVersion* result)
{
    if (!result)
        return false;

    oaa::ProtocolVersion parsed;
    if (text == QStringLiteral("1.7")) {
        parsed = oaa::kGalVersion1_7;
    } else if (text == QStringLiteral("4.3")) {
        parsed = oaa::kGalVersion4_3;
    } else if (text == QStringLiteral("5.0")) {
        parsed = oaa::kGalVersion5_0;
    } else if (text == QStringLiteral("5.1")) {
        parsed = oaa::kGalVersion5_1;
    } else if (text == QStringLiteral("6.0")) {
        parsed = oaa::kGalVersion6_0;
    } else {
        return false;
    }

    *result = parsed;
    return true;
}

oaa::ProtocolVersion resolveConfiguredGalVersion(
    const oap::YamlConfig& config)
{
    const QString configured = config.valueByPath(
        QStringLiteral("connection.gal_version")).toString();
    oaa::ProtocolVersion parsed;
    if (parseSupportedGalVersion(configured, &parsed))
        return parsed;

    qWarning().noquote()
        << "[GAL] Invalid configured version"
        << (configured.isEmpty() ? QStringLiteral("<missing>") : configured)
        << "— using highest accepted"
        << galVersionToString(kHighestAcceptedGalVersion);
    return kHighestAcceptedGalVersion;
}

QStringList supportedGalVersionStrings()
{
    return {QStringLiteral("1.7"), QStringLiteral("4.3"),
            QStringLiteral("5.0"), QStringLiteral("5.1"),
            QStringLiteral("6.0")};
}

} // namespace oap::aa
