#include <QtTest>
#include "core/api/ApiFramer.hpp"

using oap::api::ApiFramer;

static QByteArray lenPrefix(quint32 n) {
    QByteArray b(4, 0);
    b[0] = (n >> 24) & 0xFF; b[1] = (n >> 16) & 0xFF;
    b[2] = (n >> 8) & 0xFF;  b[3] = n & 0xFF;
    return b;
}

class TestApiFraming : public QObject {
    Q_OBJECT
private slots:
    void testEncodeProducesPrefix();
    void testSingleCompleteFrame();
    void testPartialThenRest();
    void testTwoFramesCoalesced();
    void testByteAtATime();
    void testOversizedLengthViolates();
    void testZeroLengthViolates();
};

void TestApiFraming::testEncodeProducesPrefix() {
    QByteArray f = ApiFramer::encode("abc");
    QCOMPARE(f.size(), 7);
    QCOMPARE(f.left(4), lenPrefix(3));
    QCOMPARE(f.mid(4), QByteArray("abc"));
}

void TestApiFraming::testSingleCompleteFrame() {
    ApiFramer fr;
    auto out = fr.feed(ApiFramer::encode("hello"));
    QCOMPARE(out.size(), 1);
    QCOMPARE(out[0], QByteArray("hello"));
    QVERIFY(!fr.violated());
}

void TestApiFraming::testPartialThenRest() {
    ApiFramer fr;
    QByteArray f = ApiFramer::encode("hello");
    QVERIFY(fr.feed(f.left(6)).isEmpty());
    auto out = fr.feed(f.mid(6));
    QCOMPARE(out.size(), 1);
    QCOMPARE(out[0], QByteArray("hello"));
}

void TestApiFraming::testTwoFramesCoalesced() {
    ApiFramer fr;
    auto out = fr.feed(ApiFramer::encode("a") + ApiFramer::encode("bb"));
    QCOMPARE(out.size(), 2);
    QCOMPARE(out[0], QByteArray("a"));
    QCOMPARE(out[1], QByteArray("bb"));
}

void TestApiFraming::testByteAtATime() {
    ApiFramer fr;
    QByteArray f = ApiFramer::encode("xyz");
    QList<QByteArray> all;
    for (char c : f) all += fr.feed(QByteArray(1, c));
    QCOMPARE(all.size(), 1);
    QCOMPARE(all[0], QByteArray("xyz"));
}

void TestApiFraming::testOversizedLengthViolates() {
    ApiFramer fr(16);
    QVERIFY(fr.feed(lenPrefix(17)).isEmpty());
    QVERIFY(fr.violated());
}

void TestApiFraming::testZeroLengthViolates() {
    ApiFramer fr;
    QVERIFY(fr.feed(lenPrefix(0)).isEmpty());
    QVERIFY(fr.violated());
}

QTEST_MAIN(TestApiFraming)
#include "test_api_framing.moc"
