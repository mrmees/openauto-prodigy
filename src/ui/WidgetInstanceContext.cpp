#include "ui/WidgetInstanceContext.hpp"

namespace oap {

WidgetInstanceContext::WidgetInstanceContext(
    const WidgetPlacement& placement,
    WidgetSize paneSize,
    IHostContext* hostContext,
    QObject* parent)
    : QObject(parent)
    , placement_(placement)
    , paneSize_(paneSize)
    , hostContext_(hostContext)
{}

} // namespace oap
