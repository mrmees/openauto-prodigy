#pragma once

#include "IConfigService.hpp"

namespace oap {

class YamlConfig;

/// Concrete IConfigService wrapping YamlConfig.
/// Single writer — only one ConfigService instance should exist.
/// Does NOT own the YamlConfig (caller manages lifetime).
class ConfigService : public IConfigService {
public:
    explicit ConfigService(YamlConfig* config, const QString& configPath);

    QVariant value(const QString& key) const override;
    void setValue(const QString& key, const QVariant& value) override;
    QVariant pluginValue(const QString& pluginId, const QString& key) const override;
    void setPluginValue(const QString& pluginId, const QString& key, const QVariant& value) override;
    void save() override;

private:
    YamlConfig* config_;
    QString configPath_;
};

} // namespace oap
