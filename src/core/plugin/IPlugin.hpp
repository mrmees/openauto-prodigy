#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QtPlugin>

class QQmlContext;

namespace oap {

class IHostContext;

class IPlugin {
public:
    virtual ~IPlugin() = default;

    // Identity
    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual int apiVersion() const = 0;

    // Lifecycle — called by PluginManager
    virtual bool initialize(IHostContext* context) = 0;
    virtual void shutdown() = 0;

    // Activation lifecycle — called when plugin becomes the active view.
    // Activation != initialization. AA starts protocol in onActivated(), not initialize().
    virtual void onActivated(QQmlContext* context) { Q_UNUSED(context); }
    virtual void onDeactivated() {}

    // UI
    virtual QUrl qmlComponent() const = 0;
    virtual QUrl iconSource() const = 0;
    virtual QUrl settingsComponent() const { return {}; }

    /// Material icon codepoint for nav strip (e.g. "\ue88a" for home).
    /// Preferred over iconSource() for font-based icons.
    virtual QString iconText() const { return {}; }

    // Capabilities
    virtual QStringList requiredServices() const = 0;

    /// If true, Shell hides status bar + nav strip when this plugin is active.
    /// Do NOT hardcode AA-specific fullscreen logic into Shell.
    virtual bool wantsFullscreen() const { return false; }
};

} // namespace oap

#define OAP_PLUGIN_IID "org.openauto.PluginInterface/1.0"
Q_DECLARE_INTERFACE(oap::IPlugin, OAP_PLUGIN_IID)
