#include "core/plugin/IPlugin.hpp"
#include <QObject>

class FixtureImposterPlugin : public QObject, public oap::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OAP_PLUGIN_IID)
    Q_INTERFACES(oap::IPlugin)
public:
    QString id() const override { return "org.test.imposter"; }
    QString name() const override { return "Imposter Fixture"; }
    QString version() const override { return "1.0"; }
    int apiVersion() const override { return 2; }
    bool initialize(oap::IHostContext*) override { return true; }
    void shutdown() override {}
    QUrl qmlComponent() const override { return {}; }
    QUrl iconSource() const override { return {}; }
    QStringList requiredServices() const override { return {}; }
};

#include "fixture_imposter_plugin.moc"
