#include "core/plugin/IPlugin.hpp"
#include <QObject>

class FixtureStalePlugin : public QObject, public oap::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OAP_PLUGIN_IID)
    Q_INTERFACES(oap::IPlugin)
public:
    QString id() const override { return "org.test.stale"; }
    QString name() const override { return "Stale Fixture"; }
    QString version() const override { return "1.0"; }
    int apiVersion() const override { return 1; }   // stale — intentionally below HOST_API_VERSION
    bool initialize(oap::IHostContext*) override { return true; }
    void shutdown() override {}
    QUrl qmlComponent() const override { return {}; }
    QUrl iconSource() const override { return {}; }
    QStringList requiredServices() const override { return {}; }
};

#include "fixture_stale_plugin.moc"
