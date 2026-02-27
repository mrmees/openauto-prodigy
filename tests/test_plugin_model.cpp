#include <QtTest>
#include <QSignalSpy>
#include <QQmlEngine>
#include "ui/PluginModel.hpp"
#include "core/plugin/PluginManager.hpp"
#include "core/plugin/IPlugin.hpp"
#include "core/plugin/IHostContext.hpp"

// Mock plugin for testing
class MockPlugin : public QObject, public oap::IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)
public:
    QString id_;
    QString name_;

    MockPlugin(const QString& id, const QString& name, QObject* parent = nullptr)
        : QObject(parent), id_(id), name_(name) {}

    QString id() const override { return id_; }
    QString name() const override { return name_; }
    QString version() const override { return "1.0"; }
    int apiVersion() const override { return 1; }
    bool initialize(oap::IHostContext*) override { return true; }
    void shutdown() override {}
    QUrl qmlComponent() const override { return {}; }
    QUrl iconSource() const override { return {}; }
    QStringList requiredServices() const override { return {}; }
};

// Minimal mock host context
class MockHostContext : public oap::IHostContext {
public:
    oap::IAudioService* audioService() override { return nullptr; }
    oap::IBluetoothService* bluetoothService() override { return nullptr; }
    oap::IConfigService* configService() override { return nullptr; }
    oap::IThemeService* themeService() override { return nullptr; }
    oap::IDisplayService* displayService() override { return nullptr; }
    oap::IEventBus* eventBus() override { return nullptr; }
    oap::ActionRegistry* actionRegistry() override { return nullptr; }
    oap::INotificationService* notificationService() override { return nullptr; }
    oap::CompanionListenerService* companionListenerService() override { return nullptr; }
    void log(oap::LogLevel, const QString&) override {}
};

class TestPluginModel : public QObject {
    Q_OBJECT
private slots:
    void testRowCountMatchesPlugins()
    {
        auto* p = new MockPlugin("test.a", "A", this);
        MockHostContext ctx;
        oap::PluginManager manager;
        QQmlEngine engine;

        manager.registerStaticPlugin(p);
        manager.initializeAll(&ctx);

        oap::PluginModel model(&manager, &engine);
        QCOMPARE(model.rowCount(), 1);
    }

    void testSetActivePluginValid()
    {
        auto* p = new MockPlugin("test.a", "A", this);
        MockHostContext ctx;
        oap::PluginManager manager;
        QQmlEngine engine;

        manager.registerStaticPlugin(p);
        manager.initializeAll(&ctx);

        oap::PluginModel model(&manager, &engine);
        QSignalSpy spy(&model, &oap::PluginModel::activePluginChanged);

        model.setActivePlugin("test.a");
        QCOMPARE(model.activePluginId(), QString("test.a"));
        QCOMPARE(spy.count(), 1);
    }

    void testSetActivePluginInvalid()
    {
        oap::PluginManager manager;
        QQmlEngine engine;
        oap::PluginModel model(&manager, &engine);

        model.setActivePlugin("nonexistent");
        QCOMPARE(model.activePluginId(), QString());  // unchanged
    }

    void testSetActivePluginEmpty()
    {
        auto* p = new MockPlugin("test.a", "A", this);
        MockHostContext ctx;
        oap::PluginManager manager;
        QQmlEngine engine;

        manager.registerStaticPlugin(p);
        manager.initializeAll(&ctx);

        oap::PluginModel model(&manager, &engine);

        model.setActivePlugin("test.a");
        model.setActivePlugin(QString());  // go home
        QCOMPARE(model.activePluginId(), QString());
    }

    void testSettingsQmlRole()
    {
        auto* p = new MockPlugin("test.a", "A", this);
        MockHostContext ctx;
        oap::PluginManager manager;
        QQmlEngine engine;

        manager.registerStaticPlugin(p);
        manager.initializeAll(&ctx);

        oap::PluginModel model(&manager, &engine);
        QModelIndex idx = model.index(0, 0);

        // MockPlugin inherits default settingsComponent(), which is empty.
        QVariant val = model.data(idx, oap::PluginModel::SettingsQmlRole);
        QVERIFY(val.isValid());
        QCOMPARE(val.toUrl(), QUrl());
    }
};

QTEST_MAIN(TestPluginModel)
#include "test_plugin_model.moc"
