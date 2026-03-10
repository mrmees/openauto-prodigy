#pragma once

#include "core/widget/WidgetTypes.hpp"
#include <QList>
#include <QMap>
#include <optional>

namespace oap {

class WidgetRegistry {
public:
    bool registerWidget(const WidgetDescriptor& descriptor);
    void unregisterWidget(const QString& widgetId);

    std::optional<WidgetDescriptor> descriptor(const QString& widgetId) const;
    QList<WidgetDescriptor> allDescriptors() const;
    QList<WidgetDescriptor> widgetsForSize(WidgetSize size) const;

private:
    QMap<QString, WidgetDescriptor> descriptors_;
};

} // namespace oap
