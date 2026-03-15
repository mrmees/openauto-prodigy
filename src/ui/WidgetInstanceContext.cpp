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
    , colSpan_(placement.colSpan)
    , rowSpan_(placement.rowSpan)
    , isCurrentPage_(false)
    , hostContext_(hostContext)
{}

void WidgetInstanceContext::setCellWidth(int v) {
    if (cellWidth_ == v) return;
    cellWidth_ = v;
    emit cellWidthChanged();
}

void WidgetInstanceContext::setCellHeight(int v) {
    if (cellHeight_ == v) return;
    cellHeight_ = v;
    emit cellHeightChanged();
}

void WidgetInstanceContext::setColSpan(int v) {
    if (colSpan_ == v) return;
    colSpan_ = v;
    emit colSpanChanged();
}

void WidgetInstanceContext::setRowSpan(int v) {
    if (rowSpan_ == v) return;
    rowSpan_ = v;
    emit rowSpanChanged();
}

void WidgetInstanceContext::setIsCurrentPage(bool v) {
    if (isCurrentPage_ == v) return;
    isCurrentPage_ = v;
    emit isCurrentPageChanged();
}

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
