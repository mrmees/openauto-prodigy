#include <QtTest>
#include "core/plugin/PluginManager.hpp"
#include "core/plugin/IPlugin.hpp"
#include "core/plugin/IHostContext.hpp"

// Mock plugin for testing
class MockPlugin : public QObject, public oap::IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)
public:
    QString id_ = "org.test.mock";
    bool initResult_ = true;
    bool initialized_ = false;
    bool shutDown_ = false;

    QString id() const override { return id_; }
    QString name() const override { return "Mock"; }
    QString version() const override { return "1.0"; }
    int apiVersion() const override { return 1; }

    bool initialize(oap::IHostContext*) override {
        initialized_ = true;
        return initResult_;
    }
    void shutdown() override { shutDown_ = true; }

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

class TestPluginManager : public QObject {
    Q_OBJECT
private slots:
    void testRegisterStaticPlugin();
    void testInitializeCallsPlugin();
    void testShutdownCallsPlugin();
    void testFailedInitDisablesPlugin();
    void testLookupById();
    void testMultiplePlugins();
    void testActivateDeactivate();
};

void TestPluginManager::testRegisterStaticPlugin()
{
    // Plugin declared BEFORE manager so it outlives mgr's destructor
    MockPlugin plugin;
    oap::PluginManager mgr;

    QSignalSpy spy(&mgr, &oap::PluginManager::pluginLoaded);
    mgr.registerStaticPlugin(&plugin);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy[0][0].toString(), QString("org.test.mock"));
}

void TestPluginManager::testInitializeCallsPlugin()
{
    MockPlugin plugin;
    MockHostContext ctx;
    oap::PluginManager mgr;

    mgr.registerStaticPlugin(&plugin);
    mgr.initializeAll(&ctx);

    QVERIFY(plugin.initialized_);
    QCOMPARE(mgr.plugins().size(), 1);
}

void TestPluginManager::testShutdownCallsPlugin()
{
    MockPlugin plugin;
    MockHostContext ctx;
    oap::PluginManager mgr;

    mgr.registerStaticPlugin(&plugin);
    mgr.initializeAll(&ctx);
    mgr.shutdownAll();

    QVERIFY(plugin.shutDown_);
    QCOMPARE(mgr.plugins().size(), 0); // no longer initialized
}

void TestPluginManager::testFailedInitDisablesPlugin()
{
    MockPlugin plugin;
    plugin.initResult_ = false;
    MockHostContext ctx;
    oap::PluginManager mgr;

    QSignalSpy failSpy(&mgr, &oap::PluginManager::pluginFailed);
    mgr.registerStaticPlugin(&plugin);
    mgr.initializeAll(&ctx);

    QCOMPARE(failSpy.count(), 1);
    QCOMPARE(mgr.plugins().size(), 0); // failed plugin not in active list
}

void TestPluginManager::testLookupById()
{
    MockPlugin plugin;
    MockHostContext ctx;
    oap::PluginManager mgr;

    mgr.registerStaticPlugin(&plugin);
    mgr.initializeAll(&ctx);

    QCOMPARE(mgr.plugin("org.test.mock"), &plugin);
    QCOMPARE(mgr.plugin("nonexistent"), nullptr);
}

void TestPluginManager::testMultiplePlugins()
{
    MockPlugin p1, p2;
    p1.id_ = "org.test.first";
    p2.id_ = "org.test.second";
    MockHostContext ctx;
    oap::PluginManager mgr;

    mgr.registerStaticPlugin(&p1);
    mgr.registerStaticPlugin(&p2);
    mgr.initializeAll(&ctx);

    QCOMPARE(mgr.plugins().size(), 2);
    QCOMPARE(mgr.plugin("org.test.first"), &p1);
    QCOMPARE(mgr.plugin("org.test.second"), &p2);
}

void TestPluginManager::testActivateDeactivate()
{
    // Plugins declared BEFORE manager so they outlive mgr's destructor
    MockPlugin plugin;
    plugin.id_ = "test.activate";
    MockPlugin plugin2;
    plugin2.id_ = "test.other";
    MockHostContext ctx;
    oap::PluginManager mgr;

    mgr.registerStaticPlugin(&plugin);
    mgr.registerStaticPlugin(&plugin2);
    mgr.initializeAll(&ctx);

    QSignalSpy activatedSpy(&mgr, &oap::PluginManager::pluginActivated);
    QSignalSpy deactivatedSpy(&mgr, &oap::PluginManager::pluginDeactivated);

    // Activate
    QVERIFY(mgr.activatePlugin("test.activate"));
    QCOMPARE(mgr.activePluginId(), QString("test.activate"));
    QCOMPARE(activatedSpy.count(), 1);
    QCOMPARE(activatedSpy.first().first().toString(), QString("test.activate"));

    // Activate different plugin deactivates previous
    QVERIFY(mgr.activatePlugin("test.other"));
    QCOMPARE(mgr.activePluginId(), QString("test.other"));
    QCOMPARE(deactivatedSpy.count(), 1);  // previous was deactivated
    QCOMPARE(activatedSpy.count(), 2);

    // Deactivate
    mgr.deactivateCurrentPlugin();
    QCOMPARE(mgr.activePluginId(), QString());
    QCOMPARE(deactivatedSpy.count(), 2);

    // Activate nonexistent plugin fails
    QVERIFY(!mgr.activatePlugin("nonexistent"));
    QCOMPARE(mgr.activePluginId(), QString());
}

QTEST_MAIN(TestPluginManager)
#include "test_plugin_manager.moc"
