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
    const QVariantMap& defaultConfig,
    const QVariantMap& instanceConfig,
    QObject* parent)
    : QObject(parent)
    , placement_(placement)
    , cellWidth_(cellWidth)
    , cellHeight_(cellHeight)
    , colSpan_(placement.colSpan)
    , rowSpan_(placement.rowSpan)
    , isCurrentPage_(false)
    , hostContext_(hostContext)
    , defaultConfig_(defaultConfig)
    , instanceConfig_(instanceConfig)
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

QVariantMap WidgetInstanceContext::effectiveConfig() const {
    QVariantMap result = defaultConfig_;
    for (auto it = instanceConfig_.constBegin(); it != instanceConfig_.constEnd(); ++it)
        result[it.key()] = it.value();
    return result;
}

void WidgetInstanceContext::setInstanceConfig(const QVariantMap& config) {
    if (instanceConfig_ == config) return;
    instanceConfig_ = config;
    emit effectiveConfigChanged();
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
