#pragma once

#include "core/plugin/IPlugin.hpp"
#include <QObject>
#include <memory>

class QQmlContext;

namespace oap {

class Configuration;
class YamlConfig;
class IHostContext;

namespace aa {
class AndroidAutoService;
class EvdevTouchReader;
}

namespace plugins {

/// Static plugin wrapping the existing Android Auto subsystem.
///
/// Lifecycle:
///   initialize() — creates AndroidAutoService + EvdevTouchReader, starts them.
///                   AA needs to listen for connections from boot, not just when visible.
///   onActivated() — exposes AA objects (service, VideoDecoder, TouchHandler)
///                    to the plugin's child QQmlContext so the QML view can bind.
///   onDeactivated() — child context is destroyed by PluginRuntimeContext.
///   shutdown() — stops touch reader + service.
///
/// Constructor takes legacy dependencies (Configuration, ApplicationController)
/// that will be replaced by IHostContext services in later phases.
class AndroidAutoPlugin : public QObject, public IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)

public:
    explicit AndroidAutoPlugin(std::shared_ptr<oap::Configuration> config,
                               oap::YamlConfig* yamlConfig = nullptr,
                               QObject* parent = nullptr);
    ~AndroidAutoPlugin() override;

    // IPlugin — Identity
    QString id() const override { return QStringLiteral("org.openauto.android-auto"); }
    QString name() const override { return QStringLiteral("Android Auto"); }
    QString version() const override { return QStringLiteral("1.0.0"); }
    int apiVersion() const override { return 1; }

    // IPlugin — Lifecycle
    bool initialize(IHostContext* context) override;
    void shutdown() override;

    // IPlugin — Activation
    void onActivated(QQmlContext* context) override;
    void onDeactivated() override;

    // IPlugin — UI
    QUrl qmlComponent() const override;
    QUrl iconSource() const override;
    QString iconText() const override { return QString(QChar(0xeff7)); }  // directions_car
    QUrl settingsComponent() const override { return {}; }

    // IPlugin — Capabilities
    QStringList requiredServices() const override { return {}; }
    bool wantsFullscreen() const override { return true; }

signals:
    void requestActivation();
    void requestDeactivation();

private:
    std::shared_ptr<oap::Configuration> config_;
    oap::YamlConfig* yamlConfig_ = nullptr;
    IHostContext* hostContext_ = nullptr;

    oap::aa::AndroidAutoService* aaService_ = nullptr;
    oap::aa::EvdevTouchReader* touchReader_ = nullptr;
};

} // namespace plugins
} // namespace oap
