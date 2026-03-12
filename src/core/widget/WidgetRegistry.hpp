#pragma once

#include <QObject>
#include "core/widget/WidgetTypes.hpp"
#include <QList>
#include <QMap>
#include <optional>

namespace oap {

class WidgetRegistry : public QObject {
    Q_OBJECT
public:
    explicit WidgetRegistry(QObject* parent = nullptr);

    bool registerWidget(const WidgetDescriptor& descriptor);
    void unregisterWidget(const QString& widgetId);

    std::optional<WidgetDescriptor> descriptor(const QString& widgetId) const;
    QList<WidgetDescriptor> allDescriptors() const;

    /// Returns descriptors where minCols <= availCols, minRows <= availRows,
    /// and qmlComponent is not empty (filters out stubs).
    QList<WidgetDescriptor> widgetsFittingSpace(int availCols, int availRows) const;

private:
    QMap<QString, WidgetDescriptor> descriptors_;
};

} // namespace oap
