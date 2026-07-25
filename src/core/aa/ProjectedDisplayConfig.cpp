#include "ProjectedDisplayConfig.hpp"

#include "../YamlConfig.hpp"

#include <QString>
#include <QVariant>

namespace oap::aa {

ProjectedClusterConfig resolveProjectedClusterConfig(const oap::YamlConfig& config)
{
    static const QString pluginId = QStringLiteral("org.openauto.android-auto");

    ProjectedClusterConfig resolved;
    resolved.enabled = config.pluginValue(
        pluginId, QStringLiteral("experimental_cluster_display")).toBool();

    const QString focus = config.pluginValue(
        pluginId, QStringLiteral("experimental_cluster_setup_focus"))
                              .toString()
                              .trimmed()
                              .toLower();
    if (focus == QStringLiteral("projected"))
        resolved.setupFocus = ProjectedSetupFocus::Projected;

    return resolved;
}

} // namespace oap::aa
