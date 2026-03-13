#include "ui/WidgetInstanceContext.hpp"
#include "core/plugin/IHostContext.hpp"
#include "core/services/IProjectionStatusProvider.hpp"
#include "core/services/INavigationProvider.hpp"
#include "core/services/IMediaStatusProvider.hpp"

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

QObject* WidgetInstanceContext::projectionStatusObj() const {
    return hostContext_ ? hostContext_->projectionStatusProvider() : nullptr;
}

QObject* WidgetInstanceContext::navigationProviderObj() const {
    return hostContext_ ? hostContext_->navigationProvider() : nullptr;
}

QObject* WidgetInstanceContext::mediaStatusObj() const {
    return hostContext_ ? hostContext_->mediaStatusProvider() : nullptr;
}

} // namespace oap
