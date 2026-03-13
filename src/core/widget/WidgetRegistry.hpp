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
    /// qmlComponent is not empty (filters out stubs), and contributionKind == Widget.
    QList<WidgetDescriptor> widgetsFittingSpace(int availCols, int availRows) const;

    /// Returns all descriptors matching the given contribution kind.
    QList<WidgetDescriptor> descriptorsByKind(DashboardContributionKind kind) const;

private:
    QMap<QString, WidgetDescriptor> descriptors_;
};

} // namespace oap
