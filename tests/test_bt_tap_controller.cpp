// tests/test_bt_tap_controller.cpp
//
// Unit coverage for BtTapController — the pure, PipeWire-free sequencing state
// machine behind the BT A2DP loopback tap (Task 7). The controller drives the
// safety-critical ordering: capture is created LAST on bring-up (it publishes
// the WirePlumber retarget target "openauto-bt-eq-in") and destroyed FIRST on
// teardown/error, so BT audio is never routed into a broken downstream. These
// tests inject a recording Effects fixture and assert the exact call order for
// the happy path, both partial bring-up failures, transport-edge toggling, and
// stream-error teardown.
#include <QtTest/QtTest>
#include <QStringList>
#include "plugins/bt_audio/BtTapController.hpp"

using oap::plugins::BtTapController;

namespace {

// A recording Effects fixture: each hook appends its tag to `log`; the three
// creating steps consult scripted booleans so partial bring-up failures are
// deterministic. Must outlive any controller built from its effects() (the
// lambdas capture `this`).
struct Recorder {
    QStringList log;
    bool acquireOk = true;
    bool playbackOk = true;
    bool captureOk = true;

    BtTapController::Effects effects() {
        BtTapController::Effects fx;
        fx.acquireEngine   = [this] { log << "acquire";        return acquireOk;  };
        fx.createPlayback  = [this] { log << "playback";       return playbackOk; };
        fx.createCapture   = [this] { log << "capture";        return captureOk;  };
        fx.destroyCapture  = [this] { log << "destroyCapture";  };
        fx.destroyPlayback = [this] { log << "destroyPlayback"; };
        fx.releaseEngine   = [this] { log << "releaseEngine";   };
        fx.activate        = [this] { log << "activate";        };
        fx.deactivate      = [this] { log << "deactivate";      };
        return fx;
    }
};

} // namespace

class TestBtTapController : public QObject {
    Q_OBJECT
private slots:
    void testStartHappyPathOrder();
    void testStartFailsAtPlaybackUnwindsEngineOnly();
    void testStartFailsAtCaptureUnwindsPlaybackThenEngine();
    void testStopFromActiveDeactivatesThenTearsDownCaptureFirst();
    void testTransportEdgesToggleOnlyWhenReady();
    void testStreamErrorTearsDownCaptureFirst();
};

// Happy path: acquire → playback → capture, state Ready, no teardown calls.
void TestBtTapController::testStartHappyPathOrder()
{
    Recorder rec;
    BtTapController ctrl(rec.effects());

    QVERIFY(ctrl.start());
    QCOMPARE(ctrl.state(), BtTapController::State::Ready);
    QCOMPARE(rec.log, QStringList({"acquire", "playback", "capture"}));
}

// Failure at playback unwinds ONLY the engine (capture never created).
void TestBtTapController::testStartFailsAtPlaybackUnwindsEngineOnly()
{
    Recorder rec;
    rec.playbackOk = false;
    BtTapController ctrl(rec.effects());

    QVERIFY(!ctrl.start());
    QCOMPARE(ctrl.state(), BtTapController::State::Stopped);
    QCOMPARE(rec.log, QStringList({"acquire", "playback", "releaseEngine"}));
    QVERIFY(!rec.log.contains("capture"));         // capture never attempted
    QVERIFY(!rec.log.contains("destroyCapture"));  // nothing to tear down
}

// Failure at capture unwinds playback THEN engine (capture-first teardown, but
// capture never existed so destroyCapture is NOT called).
void TestBtTapController::testStartFailsAtCaptureUnwindsPlaybackThenEngine()
{
    Recorder rec;
    rec.captureOk = false;
    BtTapController ctrl(rec.effects());

    QVERIFY(!ctrl.start());
    QCOMPARE(ctrl.state(), BtTapController::State::Stopped);
    QCOMPARE(rec.log, QStringList({"acquire", "playback", "capture",
                                   "destroyPlayback", "releaseEngine"}));
    QVERIFY(!rec.log.contains("destroyCapture"));  // capture create failed => no destroy
}

// stop() from Active first deactivates, then tears down capture-first.
void TestBtTapController::testStopFromActiveDeactivatesThenTearsDownCaptureFirst()
{
    Recorder rec;
    BtTapController ctrl(rec.effects());
    QVERIFY(ctrl.start());
    ctrl.onTransportActive(true);
    QCOMPARE(ctrl.state(), BtTapController::State::Active);

    ctrl.stop();
    QCOMPARE(ctrl.state(), BtTapController::State::Stopped);
    QCOMPARE(rec.log.mid(3), QStringList({"activate", "deactivate",
        "destroyCapture", "destroyPlayback", "releaseEngine"}));
}

// Transport edges toggle activity ONLY between Ready<->Active; they are ignored
// while Stopped and duplicate edges do not re-run effects.
void TestBtTapController::testTransportEdgesToggleOnlyWhenReady()
{
    Recorder rec;
    BtTapController ctrl(rec.effects());

    // Edge while Stopped is ignored — no effect, stays Stopped.
    ctrl.onTransportActive(true);
    QCOMPARE(ctrl.state(), BtTapController::State::Stopped);
    QVERIFY(rec.log.isEmpty());
    ctrl.onTransportActive(false);
    QCOMPARE(ctrl.state(), BtTapController::State::Stopped);
    QVERIFY(rec.log.isEmpty());

    QVERIFY(ctrl.start());
    const int afterStart = rec.log.size();  // 3 (acquire, playback, capture)

    // true -> Active (activate once)
    ctrl.onTransportActive(true);
    QCOMPARE(ctrl.state(), BtTapController::State::Active);
    QCOMPARE(rec.log.mid(afterStart), QStringList({"activate"}));

    // duplicate true -> no re-run
    ctrl.onTransportActive(true);
    QCOMPARE(ctrl.state(), BtTapController::State::Active);
    QCOMPARE(rec.log.mid(afterStart), QStringList({"activate"}));

    // false -> Ready (deactivate once)
    ctrl.onTransportActive(false);
    QCOMPARE(ctrl.state(), BtTapController::State::Ready);
    QCOMPARE(rec.log.mid(afterStart), QStringList({"activate", "deactivate"}));

    // duplicate false -> no re-run
    ctrl.onTransportActive(false);
    QCOMPARE(ctrl.state(), BtTapController::State::Ready);
    QCOMPARE(rec.log.mid(afterStart), QStringList({"activate", "deactivate"}));
}

// onStreamError() tears down capture-first to Stopped: from Active it first
// deactivates; from Ready (never activated) it skips deactivate.
void TestBtTapController::testStreamErrorTearsDownCaptureFirst()
{
    // From Active: deactivate then capture-first teardown.
    {
        Recorder rec;
        BtTapController ctrl(rec.effects());
        QVERIFY(ctrl.start());
        ctrl.onTransportActive(true);
        QCOMPARE(ctrl.state(), BtTapController::State::Active);

        ctrl.onStreamError();
        QCOMPARE(ctrl.state(), BtTapController::State::Stopped);
        // Index 3 is the "activate" from onTransportActive(true); teardown
        // follows from index 4 — deactivate, then capture-first.
        QCOMPARE(rec.log.mid(4), QStringList({"deactivate",
            "destroyCapture", "destroyPlayback", "releaseEngine"}));
    }

    // From Ready: no deactivate (never became active), still capture-first.
    {
        Recorder rec;
        BtTapController ctrl(rec.effects());
        QVERIFY(ctrl.start());
        QCOMPARE(ctrl.state(), BtTapController::State::Ready);

        ctrl.onStreamError();
        QCOMPARE(ctrl.state(), BtTapController::State::Stopped);
        QCOMPARE(rec.log.mid(3), QStringList({"destroyCapture",
            "destroyPlayback", "releaseEngine"}));
        QVERIFY(!rec.log.contains("deactivate"));
    }
}

QTEST_GUILESS_MAIN(TestBtTapController)
#include "test_bt_tap_controller.moc"
