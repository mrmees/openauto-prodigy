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

struct ProjectedViewportGeometry {
    int encodedWidth;
    int encodedHeight;
    int contentWidth;
    int contentHeight;

    constexpr int marginWidth() const { return encodedWidth - contentWidth; }
    constexpr int marginHeight() const { return encodedHeight - contentHeight; }
    constexpr int contentX() const { return marginWidth() / 2; }
    constexpr int contentY() const { return marginHeight() / 2; }
    constexpr bool isValid() const
    {
        return encodedWidth > 0 && encodedHeight > 0
            && contentWidth > 0 && contentHeight > 0
            && contentWidth == contentHeight
            && contentWidth <= encodedWidth
            && contentHeight <= encodedHeight
            && marginWidth() % 2 == 0 && marginHeight() % 2 == 0;
    }
};

inline constexpr ProjectedViewportGeometry kClusterViewportGeometry{
    800, 480, 300, 300
};

static_assert(kClusterViewportGeometry.isValid());

ProjectedClusterConfig resolveProjectedClusterConfig(const oap::YamlConfig& config);

inline constexpr uint8_t kMainDisplayId = 0;
inline constexpr uint8_t kClusterDisplayId = 1;

} // namespace oap::aa
