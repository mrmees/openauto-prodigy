#pragma once

#include "core/aa/ProjectedDisplayConfig.hpp"

namespace oap { class WidgetRegistry; }

namespace oap::plugins {

bool registerAAClusterWidget(oap::WidgetRegistry& registry,
                             const oap::aa::ProjectedClusterConfig& config);

} // namespace oap::plugins
