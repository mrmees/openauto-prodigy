#include <QtTest>

#include "core/plugin/IHostContext.hpp"
#include "core/services/PhoneStateService.hpp"
#include "plugins/phone/PhonePlugin.hpp"

class PhoneHostContext : public oap::IHostContext {
public:
    explicit PhoneHostContext(oap::PhoneStateService* service) : service_(service) {}

    oap::IAudioService* audioService() override { return nullptr; }
    oap::IBluetoothService* bluetoothService() override { return nullptr; }
    oap::IConfigService* configService() override { return nullptr; }
    oap::IThemeService* themeService() override { return nullptr; }
    oap::IDisplayService* displayService() override { return nullptr; }
    oap::IEventBus* eventBus() override { return nullptr; }
    oap::ActionRegistry* actionRegistry() override { return nullptr; }
    oap::INotificationService* notificationService() override { return nullptr; }
    oap::IEqualizerService* equalizerService() override { return nullptr; }
    oap::IProjectionStatusProvider* projectionStatusProvider() override { return nullptr; }
    oap::INavigationProvider* navigationProvider() override { return nullptr; }
    oap::IMediaStatusProvider* mediaStatusProvider() override { return nullptr; }
    oap::ICallStateProvider* callStateProvider() override { return service_; }
    oap::OverlayService* overlayService() override { return nullptr; }
    void log(oap::LogLevel, const QString&) override {}

private:
    oap::PhoneStateService* service_;
};

class TestPhonePlugin : public QObject {
    Q_OBJECT

private slots:
    void shutdownDisconnectsProviderBeforeClearingService();
};

void TestPhonePlugin::shutdownDisconnectsProviderBeforeClearingService()
{
    oap::PhoneStateService service;
    PhoneHostContext host(&service);
    oap::plugins::PhonePlugin plugin;
    QVERIFY(plugin.initialize(&host));
    QSignalSpy stateSpy(&plugin, &oap::plugins::PhonePlugin::callStateChanged);

    service.setIncomingCall(QStringLiteral("+15551234567"), QStringLiteral("Caller"));
    QVERIFY(service.answer());
    QCOMPARE(plugin.callState(), static_cast<int>(oap::plugins::PhonePlugin::Active));
    const int beforeShutdown = stateSpy.count();

    plugin.shutdown();
    QVERIFY(service.hangup());
    QCOMPARE(service.callState(), static_cast<int>(oap::ICallStateProvider::Idle));
    QCOMPARE(plugin.callState(), static_cast<int>(oap::plugins::PhonePlugin::Active));
    QCOMPARE(stateSpy.count(), beforeShutdown);

    // Public controls and repeated teardown remain inert after detachment.
    plugin.answer();
    plugin.hangup();
    plugin.sendDTMF(QStringLiteral("1"));
    plugin.shutdown();
}

QTEST_GUILESS_MAIN(TestPhonePlugin)
#include "test_phone_plugin.moc"
