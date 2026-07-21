// Inert-without-PipeWire safety only. Node-state tracking is live-check
// territory (design doc §11 L4) — there is no PipeWire daemon in CI.
#include <QtTest/QtTest>
#include "core/audio/ScoNodeMonitor.hpp"
#include "core/services/AudioService.hpp"

class TestScoNodeMonitor : public QObject {
    Q_OBJECT
private slots:
    void testInertWithoutPipeWire();
    void testAudioOwnerStopsMonitorBeforeTeardown();
};

void TestScoNodeMonitor::testInertWithoutPipeWire() {
    oap::ScoNodeMonitor m;
    QVERIFY(!m.scoRunning());
    m.start(nullptr, nullptr);   // must be a guarded no-op
    QVERIFY(!m.scoRunning());
    m.stop();
    m.stop();                    // idempotent
    QVERIFY(!m.scoRunning());
}

void TestScoNodeMonitor::testAudioOwnerStopsMonitorBeforeTeardown() {
    oap::ScoNodeMonitor monitor;
    auto* audio = new oap::AudioService;
    bool preTeardownObserved = false;

    connect(audio, &oap::AudioService::aboutToDestroyPipeWire,
            &monitor, &oap::ScoNodeMonitor::stop, Qt::DirectConnection);
    connect(audio, &oap::AudioService::aboutToDestroyPipeWire,
            this, [&preTeardownObserved]() { preTeardownObserved = true; },
            Qt::DirectConnection);

    if (audio->isAvailable())
        monitor.start(audio->pwThreadLoop(), audio->pwCore());

    delete audio;
    QVERIFY(preTeardownObserved);
    monitor.stop();
    QVERIFY(!monitor.scoRunning());
}

QTEST_MAIN(TestScoNodeMonitor)
#include "test_sco_node_monitor.moc"
