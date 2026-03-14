#pragma once

#include "core/plugin/IPlugin.hpp"
#include <QObject>
#include <memory>

class QQmlContext;

namespace oap {

class YamlConfig;
class IHostContext;
class DisplayInfo;

namespace aa {
class AndroidAutoOrchestrator;
class EvdevTouchReader;
class EvdevCoordBridge;
class AndroidAutoRuntimeBridge;
}

namespace plugins {

/// Static plugin wrapping the existing Android Auto subsystem.
///
/// Lifecycle:
///   initialize() — creates AndroidAutoService, delegates touch/display/navbar
///                   setup to AndroidAutoRuntimeBridge, starts AA.
///   onActivated() — exposes AA objects (service, VideoDecoder, TouchHandler)
///                    to the plugin's child QQmlContext so the QML view can bind.
///   onDeactivated() — child context is destroyed by PluginRuntimeContext.
///   shutdown() — stops touch reader + service.
class AndroidAutoPlugin : public QObject, public IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)

public:
    explicit AndroidAutoPlugin(oap::YamlConfig* yamlConfig = nullptr,
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
    QString iconText() const override { return QString(QChar(0xe859)); }  // android
    QUrl settingsComponent() const override { return {}; }

    // IPlugin — Capabilities
    QStringList requiredServices() const override { return {}; }
    bool wantsFullscreen() const override;

    /// Set detected display dimensions source (must be called before initialize)
    void setDisplayInfo(DisplayInfo* info);

    /// Gracefully disconnect the AA session (sends ShutdownRequest to phone)
    void stopAA();

    /// Access the runtime bridge (for zone registration, coord bridge access)
    oap::aa::AndroidAutoRuntimeBridge* runtimeBridge() const { return runtimeBridge_; }

    /// Access the evdev coordinate bridge for external zone registration
    oap::aa::EvdevCoordBridge* coordBridge() const;

    /// Access the AA orchestrator (for root context exposure)
    oap::aa::AndroidAutoOrchestrator* orchestrator() const { return aaService_; }

public slots:
    void onConfigChanged(const QString& path, const QVariant& value);

signals:
    void requestActivation();
    void requestDeactivation();
    void gestureTriggered();

private:
    oap::YamlConfig* yamlConfig_ = nullptr;
    IHostContext* hostContext_ = nullptr;

    oap::DisplayInfo* displayInfo_ = nullptr;
    oap::aa::AndroidAutoOrchestrator* aaService_ = nullptr;
    oap::aa::AndroidAutoRuntimeBridge* runtimeBridge_ = nullptr;
};

} // namespace plugins
} // namespace oap
