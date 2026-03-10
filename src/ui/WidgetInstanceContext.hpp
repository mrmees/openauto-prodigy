#pragma once

#include <QObject>
#include "core/widget/WidgetTypes.hpp"

namespace oap {

class IHostContext;

class WidgetInstanceContext : public QObject {
    Q_OBJECT
    Q_PROPERTY(int paneSize READ paneSizeInt CONSTANT)
    Q_PROPERTY(QString instanceId READ instanceId CONSTANT)
    Q_PROPERTY(QString widgetId READ widgetId CONSTANT)

public:
    WidgetInstanceContext(const WidgetPlacement& placement,
                          WidgetSize paneSize,
                          IHostContext* hostContext,
                          QObject* parent = nullptr);

    WidgetSize paneSize() const { return paneSize_; }
    int paneSizeInt() const { return static_cast<int>(paneSize_); }
    QString instanceId() const { return placement_.instanceId; }
    QString widgetId() const { return placement_.widgetId; }
    IHostContext* hostContext() const { return hostContext_; }
    const WidgetPlacement& placement() const { return placement_; }

private:
    WidgetPlacement placement_;
    WidgetSize paneSize_;
    IHostContext* hostContext_;
};

} // namespace oap
