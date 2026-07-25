#include "AAClusterWidgetRegistration.hpp"

#include "core/widget/WidgetRegistry.hpp"

namespace oap::plugins {

bool registerAAClusterWidget(
    oap::WidgetRegistry& registry,
    const oap::aa::ProjectedClusterConfig& config)
{
    if (!config.enabled)
        return false;

    oap::WidgetDescriptor descriptor;
    descriptor.id = QStringLiteral("org.openauto.aa-cluster");
    descriptor.displayName = QStringLiteral("Android Auto Cluster");
    descriptor.iconName = QStringLiteral("\ue55c"); // navigation
    descriptor.category = QStringLiteral("navigation");
    descriptor.description =
        QStringLiteral("Projected Android Auto cluster display");
    descriptor.qmlComponent = QUrl(
        QStringLiteral("qrc:/OpenAutoProdigy/AAClusterWidget.qml"));
    descriptor.minCols = 3;
    descriptor.maxCols = 3;
    descriptor.defaultCols = 3;
    descriptor.minRows = 3;
    descriptor.maxRows = 3;
    descriptor.defaultRows = 3;
    return registry.registerWidget(descriptor);
}

} // namespace oap::plugins
