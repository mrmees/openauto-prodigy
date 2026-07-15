#include <QTest>
#include <QSignalSpy>
#include "core/aa/AndroidAutoOrchestrator.hpp"
#include "core/services/IConfigService.hpp"

// Stub IConfigService for testing
class StubConfigService : public oap::IConfigService {
public:
    QVariantMap values;

    QVariant value(const QString& key) const override {
        return values.value(key);
    }
    void setValue(const QString& key, const QVariant& v) override {
        values[key] = v;
    }
    QVariant pluginValue(const QString&, const QString&) const override { return {}; }
    void setPluginValue(const QString&, const QString&, const QVariant&) override {}
    void save() override {}
};

class TestAndroidAutoOrchestrator : public QObject {
    Q_OBJECT
private slots:
    void testInitialState() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Disconnected));
        QVERIFY(orch.videoDecoder() != nullptr);
        QVERIFY(orch.inputHandler() != nullptr);
    }

    void testStartListens() {
        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 15277;  // Use non-standard port to avoid conflicts
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        QSignalSpy stateSpy(&orch, &oap::aa::AndroidAutoOrchestrator::connectionStateChanged);

        orch.start();

        // Should transition to WaitingForDevice
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::WaitingForDevice));
        QVERIFY(stateSpy.count() >= 1);

        orch.stop();
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Disconnected));
    }

    void testStartUsesDefaultPortWhenNotConfigured() {
        // When connection.tcp_port is not set, should use default 5277
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        // Just verify it doesn't crash; the port is internal
        orch.start();
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::WaitingForDevice));
        orch.stop();
    }

    void testTcpPortFromConfigService() {
        // Verify the orchestrator reads connection.tcp_port from IConfigService
        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 15278;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        orch.start();

        // Check status message includes our configured port
        QVERIFY(orch.statusMessage().contains("15278"));

        orch.stop();
    }

    void testStopWithoutStart() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        // Should not crash
        orch.stop();
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Disconnected));
    }

    void testVideoFocusWithoutConnection() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        // Should not crash when not connected
        orch.requestVideoFocus();
        orch.requestExitToCar();
    }

    void testPhonePropertiesDefaultValues() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        QCOMPARE(orch.phoneBatteryLevel(), -1);
        QCOMPARE(orch.phoneSignalStrength(), -1);
    }

    void testAaConnectedFalseWhenDisconnected() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        QVERIFY(!orch.isAaConnected());
    }
};

QTEST_MAIN(TestAndroidAutoOrchestrator)
#include "test_aa_orchestrator.moc"
