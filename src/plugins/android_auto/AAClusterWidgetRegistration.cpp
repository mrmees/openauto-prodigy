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
    descriptor.minCols = 2;
    descriptor.maxCols = 2;
    descriptor.defaultCols = 2;
    descriptor.minRows = 2;
    descriptor.maxRows = 2;
    descriptor.defaultRows = 2;
    return registry.registerWidget(descriptor);
}

} // namespace oap::plugins
