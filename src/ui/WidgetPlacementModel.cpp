#include "ui/WidgetPlacementModel.hpp"
#include "core/widget/WidgetRegistry.hpp"

namespace oap {

WidgetPlacementModel::WidgetPlacementModel(WidgetRegistry* registry, QObject* parent)
    : QObject(parent), registry_(registry) {}

void WidgetPlacementModel::setActivePageId(const QString& pageId) {
    if (activePageId_ == pageId) return;
    activePageId_ = pageId;
    emit activePageIdChanged();
}

void WidgetPlacementModel::setPlacements(const QList<WidgetPlacement>& placements) {
    placements_ = placements;
    emit placementsChanged();
}

std::optional<WidgetPlacement> WidgetPlacementModel::placementForPane(const QString& paneId) const {
    for (const auto& p : placements_) {
        if (p.pageId == activePageId_ && p.paneId == paneId)
            return p;
    }
    return std::nullopt;
}

QUrl WidgetPlacementModel::qmlComponentForPane(const QString& paneId) const {
    auto placement = placementForPane(paneId);
    if (!placement) return {};
    auto desc = registry_->descriptor(placement->widgetId);
    if (!desc) return {};
    return desc->qmlComponent;
}

void WidgetPlacementModel::swapWidget(const QString& paneId, const QString& widgetId) {
    // Remove existing placement for this page+pane
    placements_.erase(
        std::remove_if(placements_.begin(), placements_.end(),
            [&](const WidgetPlacement& p) {
                return p.pageId == activePageId_ && p.paneId == paneId;
            }),
        placements_.end());

    // Add new placement
    WidgetPlacement newPlacement;
    newPlacement.instanceId = widgetId + "-" + paneId;
    newPlacement.widgetId = widgetId;
    newPlacement.pageId = activePageId_;
    newPlacement.paneId = paneId;
    placements_.append(newPlacement);

    emit placementsChanged();
    emit paneChanged(paneId);
}

void WidgetPlacementModel::clearPane(const QString& paneId) {
    placements_.erase(
        std::remove_if(placements_.begin(), placements_.end(),
            [&](const WidgetPlacement& p) {
                return p.pageId == activePageId_ && p.paneId == paneId;
            }),
        placements_.end());

    emit placementsChanged();
    emit paneChanged(paneId);
}

} // namespace oap
