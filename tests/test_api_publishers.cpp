#include <QtTest>
#include <QSignalSpy>
#include <QDateTime>

#include "core/api/ApiPublishers.hpp"
#include "core/services/MediaStatusService.hpp"
#include "core/services/PhoneStateService.hpp"

namespace pb = prodigy::api::v1;
using oap::api::MediaPublisher;
using oap::api::PhonePublisher;
using oap::api::TopicPublisher;

class TestApiPublishers : public QObject {
    Q_OBJECT
private slots:
    void testSnapshotBytesParses();
    void testCoalescingCollapsesBursts();
    void testSeparateTurnsSeparateEmits();
    void testPhoneStartedAtSynthesis();
    void testPhoneStartedAtRecomputedAfterClockAdjustment();
    void testPhoneCapabilityChangeEmits();
};

void TestApiPublishers::testSnapshotBytesParses() {
    oap::MediaStatusService media;
    MediaPublisher pub(&media);

    QByteArray bytes = pub.snapshotBytes();
    pb::ApiMessage msg;
    QVERIFY(msg.ParseFromArray(bytes.constData(), bytes.size()));
    QCOMPARE(msg.payload_case(), pb::ApiMessage::kMediaStatus);
    QCOMPARE(msg.request_id(), quint64(0));
}

void TestApiPublishers::testCoalescingCollapsesBursts() {
    oap::MediaStatusService media;
    media.setBtConnected(true);
    MediaPublisher pub(&media);

    QSignalSpy spy(&pub, &TopicPublisher::statusReady);

    // Two mediaStatusChanged emissions in the same event-loop turn.
    media.updateBtMetadata("a", "b", "c");
    media.updateBtPlaybackState(1);

    QTest::qWait(20);
    QCOMPARE(spy.count(), 1);
}

void TestApiPublishers::testSeparateTurnsSeparateEmits() {
    oap::MediaStatusService media;
    media.setBtConnected(true);
    MediaPublisher pub(&media);

    QSignalSpy spy(&pub, &TopicPublisher::statusReady);

    media.updateBtMetadata("a", "b", "c");
    QTest::qWait(20);
    media.updateBtPlaybackState(1);
    QTest::qWait(20);

    QCOMPARE(spy.count(), 2);
}

void TestApiPublishers::testPhoneStartedAtSynthesis() {
    oap::PhoneStateService phone;
    PhonePublisher pub(&phone);

    QSignalSpy spy(&pub, &TopicPublisher::statusReady);

    phone.setIncomingCall("+15551234567", "Alice");
    QTest::qWait(20);
    QVERIFY(phone.answer());
    QTest::qWait(20);

    QVERIFY(spy.count() > 0);
    QByteArray lastBytes = spy.last().at(1).toByteArray();
    pb::ApiMessage msg;
    QVERIFY(msg.ParseFromArray(lastBytes.constData(), lastBytes.size()));
    QCOMPARE(msg.payload_case(), pb::ApiMessage::kPhoneStatus);
    QCOMPARE(msg.phone_status().calls_size(), 1);

    const qint64 startedAt = msg.phone_status().calls(0).started_at_unix_ms();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(qAbs(now - startedAt) < 5000);

    spy.clear();
    QVERIFY(phone.hangup());
    QTest::qWait(20);

    QVERIFY(spy.count() > 0);
    QByteArray hangupBytes = spy.last().at(1).toByteArray();
    pb::ApiMessage hangupMsg;
    QVERIFY(hangupMsg.ParseFromArray(hangupBytes.constData(), hangupBytes.size()));
    QCOMPARE(hangupMsg.phone_status().calls_size(), 0);
}

void TestApiPublishers::testPhoneStartedAtRecomputedAfterClockAdjustment() {
    oap::PhoneStateService phone;
    PhonePublisher pub(&phone);
    qint64 now = Q_INT64_C(1752000000000);
    pub.setClockForTest([&now] { return now; });
    QSignalSpy spy(&pub, &TopicPublisher::statusReady);

    phone.setIncomingCall("+15551234567", "Alice");
    QVERIFY(phone.answer());
    QTest::qWait(20);
    spy.clear();

    now += 3600000;
    pub.onSystemClockAdjusted();
    QTest::qWait(20);

    QCOMPARE(spy.count(), 1);
    pb::ApiMessage msg;
    const QByteArray bytes = spy.takeFirst().at(1).toByteArray();
    QVERIFY(msg.ParseFromArray(bytes.constData(), bytes.size()));
    QCOMPARE(msg.phone_status().calls_size(), 1);
    QCOMPARE(msg.phone_status().calls(0).started_at_unix_ms(),
             now - qint64(phone.callDuration()) * 1000);
}

// ADAPTATION (controller-resolved, HFP phone-seam override): capabilities
// are status too -- toggling telephonyAvailable() in mock mode must reach
// subscribers via the same coalesced statusReady stream, without needing a
// call in progress.
void TestApiPublishers::testPhoneCapabilityChangeEmits() {
    oap::PhoneStateService phone;
    PhonePublisher pub(&phone);

    QSignalSpy spy(&pub, &TopicPublisher::statusReady);

    phone.onTelephonyAvailable(true);
    QTest::qWait(20);

    QCOMPARE(spy.count(), 1);
    QByteArray bytes = spy.last().at(1).toByteArray();
    pb::ApiMessage msg;
    QVERIFY(msg.ParseFromArray(bytes.constData(), bytes.size()));
    QCOMPARE(msg.payload_case(), pb::ApiMessage::kPhoneStatus);
    QVERIFY(msg.phone_status().capabilities().can_dial());
}

QTEST_MAIN(TestApiPublishers)
#include "test_api_publishers.moc"
