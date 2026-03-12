#include "core/widget/WidgetRegistry.hpp"

namespace oap {

WidgetRegistry::WidgetRegistry(QObject* parent) : QObject(parent) {}

bool WidgetRegistry::registerWidget(const WidgetDescriptor& descriptor) {
    if (descriptors_.contains(descriptor.id))
        return false;
    descriptors_.insert(descriptor.id, descriptor);
    return true;
}

void WidgetRegistry::unregisterWidget(const QString& widgetId) {
    descriptors_.remove(widgetId);
}

std::optional<WidgetDescriptor> WidgetRegistry::descriptor(const QString& widgetId) const {
    auto it = descriptors_.find(widgetId);
    if (it == descriptors_.end())
        return std::nullopt;
    return *it;
}

QList<WidgetDescriptor> WidgetRegistry::allDescriptors() const {
    return descriptors_.values();
}

QList<WidgetDescriptor> WidgetRegistry::widgetsFittingSpace(int availCols, int availRows) const {
    QList<WidgetDescriptor> result;
    for (const auto& desc : descriptors_) {
        if (desc.minCols <= availCols && desc.minRows <= availRows
            && !desc.qmlComponent.isEmpty()) {
            result.append(desc);
        }
    }
    return result;
}

} // namespace oap
