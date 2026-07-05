#include <QtTest/QtTest>
#include "core/services/CallAudioPolicy.hpp"
#include "core/services/IProjectionStatusProvider.hpp"

class FakeProjection : public oap::IProjectionStatusProvider {
    Q_OBJECT
public:
    using oap::IProjectionStatusProvider::IProjectionStatusProvider;
    int projectionState() const override { return state_; }
    QString statusMessage() const override { return {}; }
    void setState(int s) { state_ = s; emit projectionStateChanged(); }
private:
    int state_ = 0;
};

class TestCallAudioPolicy : public QObject {
    Q_OBJECT
private slots:
    void testDisabledNeverRejects();
    void testEnabledFollowsProjection();
    void testNullProviderNeverRejects();
};

void TestCallAudioPolicy::testDisabledNeverRejects() {
    FakeProjection proj;
    oap::CallAudioPolicy p(&proj, /*rejectScoDuringAa=*/false);
    QSignalSpy spy(&p, &oap::CallAudioPolicy::rejectScoWanted);
    QVERIFY(!p.wantReject());
    proj.setState(oap::IProjectionStatusProvider::Connected);
    QVERIFY(!p.wantReject());
    QCOMPARE(spy.count(), 0);
}

void TestCallAudioPolicy::testEnabledFollowsProjection() {
    FakeProjection proj;
    oap::CallAudioPolicy p(&proj, true);
    QSignalSpy spy(&p, &oap::CallAudioPolicy::rejectScoWanted);
    QVERIFY(!p.wantReject());                                     // Disconnected
    proj.setState(oap::IProjectionStatusProvider::Connecting);
    QVERIFY(!p.wantReject());                                     // not yet a session
    proj.setState(oap::IProjectionStatusProvider::Connected);
    QVERIFY(p.wantReject());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).toBool(), true);
    proj.setState(oap::IProjectionStatusProvider::Backgrounded);  // still a session
    QVERIFY(p.wantReject());
    QCOMPARE(spy.count(), 1);                                     // no re-emit, unchanged
    proj.setState(oap::IProjectionStatusProvider::Disconnected);
    QVERIFY(!p.wantReject());
    QCOMPARE(spy.count(), 2);
}

void TestCallAudioPolicy::testNullProviderNeverRejects() {
    oap::CallAudioPolicy p(nullptr, true);
    QVERIFY(!p.wantReject());
}

QTEST_MAIN(TestCallAudioPolicy)
#include "test_call_audio_policy.moc"
