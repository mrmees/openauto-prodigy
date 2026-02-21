#include "ConfigService.hpp"
#include "core/YamlConfig.hpp"

namespace oap {

ConfigService::ConfigService(YamlConfig* config, const QString& configPath)
    : config_(config), configPath_(configPath)
{
}

QVariant ConfigService::value(const QString& key) const
{
    return config_->valueByPath(key);
}

void ConfigService::setValue(const QString& key, const QVariant& val)
{
    config_->setValueByPath(key, val);
}

QVariant ConfigService::pluginValue(const QString& pluginId, const QString& key) const
{
    return config_->pluginValue(pluginId, key);
}

void ConfigService::setPluginValue(const QString& pluginId, const QString& key, const QVariant& value)
{
    config_->setPluginValue(pluginId, key, value);
}

void ConfigService::save()
{
    config_->save(configPath_);
}

} // namespace oap
