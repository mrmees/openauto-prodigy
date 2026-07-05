// Inert-without-PipeWire safety only. Node-state tracking is live-check
// territory (design doc §11 L4) — there is no PipeWire daemon in CI.
#include <QtTest/QtTest>
#include "core/audio/ScoNodeMonitor.hpp"

class TestScoNodeMonitor : public QObject {
    Q_OBJECT
private slots:
    void testInertWithoutPipeWire();
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

QTEST_MAIN(TestScoNodeMonitor)
#include "test_sco_node_monitor.moc"
