#pragma once

#include <cstdint>

namespace oap { class YamlConfig; }

namespace oap::aa {

enum class ProjectedDisplayRole {
    Main,
    Cluster,
};

enum class ProjectedSetupFocus {
    ProjectedNoInput,
    Projected,
};

struct ProjectedClusterConfig {
    bool enabled = false;
    ProjectedSetupFocus setupFocus = ProjectedSetupFocus::ProjectedNoInput;
};

ProjectedClusterConfig resolveProjectedClusterConfig(const oap::YamlConfig& config);

inline constexpr uint8_t kMainDisplayId = 0;
inline constexpr uint8_t kClusterDisplayId = 1;

} // namespace oap::aa
