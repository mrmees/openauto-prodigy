#include "ui/WidgetInstanceContext.hpp"

namespace oap {

WidgetInstanceContext::WidgetInstanceContext(
    const GridPlacement& placement,
    int cellWidth,
    int cellHeight,
    IHostContext* hostContext,
    QObject* parent)
    : QObject(parent)
    , placement_(placement)
    , cellWidth_(cellWidth)
    , cellHeight_(cellHeight)
    , hostContext_(hostContext)
{}

} // namespace oap
