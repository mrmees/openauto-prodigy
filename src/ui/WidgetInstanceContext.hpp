#pragma once

#include <QObject>
#include "core/widget/WidgetTypes.hpp"

namespace oap {

class IHostContext;

class WidgetInstanceContext : public QObject {
    Q_OBJECT
    Q_PROPERTY(int cellWidth READ cellWidth CONSTANT)
    Q_PROPERTY(int cellHeight READ cellHeight CONSTANT)
    Q_PROPERTY(QString instanceId READ instanceId CONSTANT)
    Q_PROPERTY(QString widgetId READ widgetId CONSTANT)

public:
    WidgetInstanceContext(const GridPlacement& placement,
                          int cellWidth,
                          int cellHeight,
                          IHostContext* hostContext,
                          QObject* parent = nullptr);

    int cellWidth() const { return cellWidth_; }
    int cellHeight() const { return cellHeight_; }
    QString instanceId() const { return placement_.instanceId; }
    QString widgetId() const { return placement_.widgetId; }
    IHostContext* hostContext() const { return hostContext_; }
    const GridPlacement& placement() const { return placement_; }

private:
    GridPlacement placement_;
    int cellWidth_;
    int cellHeight_;
    IHostContext* hostContext_;
};

} // namespace oap
