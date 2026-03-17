#include "ui/WidgetContextFactory.hpp"
#include "ui/WidgetGridModel.hpp"
#include "ui/WidgetInstanceContext.hpp"
#include "core/widget/WidgetRegistry.hpp"
#include "core/plugin/IHostContext.hpp"

namespace oap {

WidgetContextFactory::WidgetContextFactory(WidgetGridModel* gridModel,
                                           IHostContext* hostContext,
                                           QObject* parent)
    : QObject(parent)
    , gridModel_(gridModel)
    , hostContext_(hostContext)
{
    if (gridModel_) {
        connect(gridModel_, &WidgetGridModel::widgetConfigChanged,
                this, [this](const QString& instanceId, const QVariantMap& /*effectiveConfig*/) {
            const auto contexts = activeContexts_.value(instanceId);
            for (auto* ctx : contexts) {
                // Pass raw overrides (not merged effective) to setInstanceConfig.
                // The context's effectiveConfig() getter handles merging with defaults.
                ctx->setInstanceConfig(gridModel_->widgetConfig(instanceId));
            }
        });
    }
}

QObject* WidgetContextFactory::createContext(const QString& instanceId, QObject* parent) {
    if (!gridModel_) return nullptr;

    // Find the placement for this instanceId
    const auto& placements = gridModel_->placements();
    for (const auto& p : placements) {
        if (p.instanceId == instanceId) {
            // Look up defaultConfig from descriptor
            QVariantMap defaultCfg;
            if (auto* reg = gridModel_->registry()) {
                auto desc = reg->descriptor(p.widgetId);
                if (desc) defaultCfg = desc->defaultConfig;
            }

            auto* ctx = new WidgetInstanceContext(p, cellSide_, cellSide_, hostContext_,
                                                   defaultCfg, p.config, parent);

            // Track this context for live config updates
            activeContexts_[instanceId].insert(ctx);

            // Clean up tracking when context is destroyed (delegate removed/recycled).
            connect(ctx, &QObject::destroyed, this, [this, instanceId, ctx]() {
                auto it = activeContexts_.find(instanceId);
                if (it == activeContexts_.end())
                    return;

                it->remove(ctx);
                if (it->isEmpty())
                    activeContexts_.remove(instanceId);
            });

            return ctx;
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
