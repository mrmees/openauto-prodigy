#pragma once

#include <QString>
#include <QStringList>

#include <oaa/Session/SessionProtocolPolicy.hpp>

namespace oap {
class YamlConfig;
}

namespace oap::aa {

inline constexpr oaa::ProtocolVersion kHighestAcceptedGalVersion =
    oaa::kGalVersion6_0;

QString galVersionToString(oaa::ProtocolVersion version);
bool parseSupportedGalVersion(const QString& text,
                              oaa::ProtocolVersion* result);
oaa::ProtocolVersion resolveConfiguredGalVersion(
    const oap::YamlConfig& config);
QStringList supportedGalVersionStrings();

} // namespace oap::aa
