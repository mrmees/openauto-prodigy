#pragma once

#include <yaml-cpp/yaml.h>

namespace oap {

// Deep merge: overlay values override base values.
// Mappings recurse, sequences and scalars override entirely,
// missing keys in overlay preserve base defaults.
inline YAML::Node mergeYaml(const YAML::Node& base, const YAML::Node& overlay)
{
    if (!overlay.IsDefined() || overlay.IsNull())
        return YAML::Clone(base);

    if (!base.IsDefined() || base.IsNull())
        return YAML::Clone(overlay);

    if (base.IsMap() && overlay.IsMap()) {
        YAML::Node result = YAML::Clone(base);
        for (auto it = overlay.begin(); it != overlay.end(); ++it) {
            const auto key = it->first.as<std::string>();
            if (result[key])
                result[key] = mergeYaml(result[key], it->second);
            else
                result[key] = YAML::Clone(it->second);
        }
        return result;
    }

    // Sequences and scalars: overlay wins entirely
    return YAML::Clone(overlay);
}

} // namespace oap
