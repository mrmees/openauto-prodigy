#include <QTest>
#include <QSignalSpy>
#include "core/aa/AndroidAutoOrchestrator.hpp"
#include "core/Configuration.hpp"

class TestAndroidAutoOrchestrator : public QObject {
    Q_OBJECT
private slots:
    void testInitialState() {
        auto config = std::make_shared<oap::Configuration>();
        oap::aa::AndroidAutoOrchestrator orch(config, nullptr, nullptr);
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Disconnected));
        QVERIFY(orch.videoDecoder() != nullptr);
        QVERIFY(orch.inputHandler() != nullptr);
    }

    void testStartListens() {
        auto config = std::make_shared<oap::Configuration>();
        config->setTcpPort(15277);  // Use non-standard port to avoid conflicts
        oap::aa::AndroidAutoOrchestrator orch(config, nullptr, nullptr);
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

    void testStopWithoutStart() {
        auto config = std::make_shared<oap::Configuration>();
        oap::aa::AndroidAutoOrchestrator orch(config, nullptr, nullptr);
        // Should not crash
        orch.stop();
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Disconnected));
    }

    void testVideoFocusWithoutConnection() {
        auto config = std::make_shared<oap::Configuration>();
        oap::aa::AndroidAutoOrchestrator orch(config, nullptr, nullptr);
        // Should not crash when not connected
        orch.requestVideoFocus();
        orch.requestExitToCar();
    }
};

QTEST_MAIN(TestAndroidAutoOrchestrator)
#include "test_aa_orchestrator.moc"
