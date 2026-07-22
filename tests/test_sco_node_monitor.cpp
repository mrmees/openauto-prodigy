// Inert-without-PipeWire safety only. Node-state tracking is live-check
// territory (design doc §11 L4) — there is no PipeWire daemon in CI.
#include <QtTest/QtTest>
#include "core/audio/ScoNodeMonitor.hpp"
#include "core/services/AudioService.hpp"

namespace oap {

class ScoNodeMonitorTestAccess {
public:
    static void activate(ScoNodeMonitor& monitor) {
        monitor.epoch_.store(1, std::memory_order_release);
        monitor.active_.store(true, std::memory_order_release);
    }

    static void observe(ScoNodeMonitor& monitor, bool running) {
        monitor.updateRunning(running);
    }
};

} // namespace oap

class TestScoNodeMonitor : public QObject {
    Q_OBJECT
private slots:
    void testInertWithoutPipeWire();
    void testAudioOwnerStopsMonitorBeforeTeardown();
    void testStopDeliversQueuedFallingEdgeExactlyOnce();
    void testStopDeliversSnapshotFallingEdgeExactlyOnce();
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

void TestScoNodeMonitor::testStopDeliversQueuedFallingEdgeExactlyOnce() {
    oap::ScoNodeMonitor monitor;
    QSignalSpy spy(&monitor, &oap::ScoNodeMonitor::scoRunningChanged);

    // Model the PW callback sequence without requiring a daemon: first deliver
    // RUNNING=true, then observe false and stop before that queued edge runs.
    oap::ScoNodeMonitorTestAccess::activate(monitor);

    oap::ScoNodeMonitorTestAccess::observe(monitor, true);
    QCoreApplication::processEvents();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);

    oap::ScoNodeMonitorTestAccess::observe(monitor, false);
    monitor.stop();
    monitor.stop();
    QCoreApplication::processEvents();

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), false);
}

void TestScoNodeMonitor::testStopDeliversSnapshotFallingEdgeExactlyOnce() {
    oap::ScoNodeMonitor monitor;
    QSignalSpy spy(&monitor, &oap::ScoNodeMonitor::scoRunningChanged);
    oap::ScoNodeMonitorTestAccess::activate(monitor);

    // PhoneStateService connects and then snapshots scoRunning(). If stop runs
    // before this queued rising edge, the snapshot still requires a false edge.
    oap::ScoNodeMonitorTestAccess::observe(monitor, true);
    QVERIFY(monitor.scoRunning());

    monitor.stop();
    monitor.stop();
    QCoreApplication::processEvents();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), false);
}

QTEST_MAIN(TestScoNodeMonitor)
#include "test_sco_node_monitor.moc"
