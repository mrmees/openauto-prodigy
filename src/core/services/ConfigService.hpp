#pragma once

#include <QObject>
#include "IConfigService.hpp"

namespace oap {

class YamlConfig;

/// Concrete IConfigService wrapping YamlConfig.
/// Single writer — only one ConfigService instance should exist.
/// Does NOT own the YamlConfig (caller manages lifetime).
class ConfigService : public QObject, public IConfigService {
    Q_OBJECT
public:
    explicit ConfigService(YamlConfig* config, const QString& configPath);

    Q_INVOKABLE QVariant value(const QString& key) const override;
    Q_INVOKABLE void setValue(const QString& key, const QVariant& value) override;
    Q_INVOKABLE QVariant pluginValue(const QString& pluginId, const QString& key) const override;
    Q_INVOKABLE void setPluginValue(const QString& pluginId, const QString& key, const QVariant& value) override;
    Q_INVOKABLE void save() override;

signals:
    void configChanged(const QString& path, const QVariant& value);

private:
    YamlConfig* config_;
    QString configPath_;
};

} // namespace oap
