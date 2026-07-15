#include "core/plugin/IPlugin.hpp"
#include <QObject>
#include <stdexcept>

class FixtureThrowingPlugin : public QObject, public oap::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OAP_PLUGIN_IID)
    Q_INTERFACES(oap::IPlugin)
public:
    QString id() const override { return "org.test.throwing"; }
    QString name() const override { return "Throwing Fixture"; }
    QString version() const override { return "1.0"; }
    // Untrusted binary metadata: exercise the host's exception guard.
    int apiVersion() const override { throw std::runtime_error("metadata bomb"); }
    bool initialize(oap::IHostContext*) override { return true; }
    void shutdown() override {}
    QUrl qmlComponent() const override { return {}; }
    QUrl iconSource() const override { return {}; }
    QStringList requiredServices() const override { return {}; }
};

#include "fixture_throwing_plugin.moc"
