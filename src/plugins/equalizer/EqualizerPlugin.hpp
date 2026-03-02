#pragma once

#include "core/plugin/IPlugin.hpp"
#include <QObject>

class QQmlContext;

namespace oap {

class IHostContext;

namespace plugins {

class EqualizerPlugin : public QObject, public IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)

public:
    explicit EqualizerPlugin(QObject* parent = nullptr);

    // IPlugin — Identity
    QString id() const override { return QStringLiteral("org.openauto.equalizer"); }
    QString name() const override { return QStringLiteral("Equalizer"); }
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
    QUrl iconSource() const override { return {}; }
    QString iconText() const override { return QString(QChar(0xe050)); }  // equalizer bars
    QUrl settingsComponent() const override { return {}; }

    // IPlugin — Capabilities
    QStringList requiredServices() const override { return {}; }
    bool wantsFullscreen() const override { return false; }

private:
    IHostContext* hostContext_ = nullptr;
};

} // namespace plugins
} // namespace oap
