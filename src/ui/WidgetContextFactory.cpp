#include "ui/WidgetContextFactory.hpp"
#include "ui/WidgetGridModel.hpp"
#include "ui/WidgetInstanceContext.hpp"
#include "core/plugin/IHostContext.hpp"

namespace oap {

WidgetContextFactory::WidgetContextFactory(WidgetGridModel* gridModel,
                                           IHostContext* hostContext,
                                           QObject* parent)
    : QObject(parent)
    , gridModel_(gridModel)
    , hostContext_(hostContext)
{}

QObject* WidgetContextFactory::createContext(const QString& instanceId, QObject* parent) {
    if (!gridModel_) return nullptr;

    // Find the placement for this instanceId
    const auto& placements = gridModel_->placements();
    for (const auto& p : placements) {
        if (p.instanceId == instanceId) {
            // Use cellSide for both width and height (square cells)
            return new WidgetInstanceContext(p, cellSide_, cellSide_, hostContext_, parent);
        }
    }

    return nullptr;
}

void WidgetContextFactory::setCellSide(int v) {
    if (cellSide_ == v) return;
    cellSide_ = v;
    emit cellSideChanged();
}

} // namespace oap
