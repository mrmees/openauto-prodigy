#include <QtTest>
#include "api/api.pb.h"

namespace pb = prodigy::api::v1;

class TestApiProtoRoundtrip : public QObject {
    Q_OBJECT
private slots:
    void testEnvelopeRoundtrip();
    void testOneofExclusivity();
};

void TestApiProtoRoundtrip::testEnvelopeRoundtrip() {
    pb::ApiMessage msg;
    msg.set_request_id(42);
    auto* hello = msg.mutable_client_hello();
    hello->set_requested_api_version_major(1);
    hello->set_client_name("test");
    hello->set_client_kind(pb::CLIENT_KIND_DIAGNOSTIC);

    std::string bytes;
    QVERIFY(msg.SerializeToString(&bytes));

    pb::ApiMessage parsed;
    QVERIFY(parsed.ParseFromString(bytes));
    QCOMPARE(parsed.request_id(), (quint64)42);
    QCOMPARE(parsed.payload_case(), pb::ApiMessage::kClientHello);
    QCOMPARE(QString::fromStdString(parsed.client_hello().client_name()), QString("test"));
}

void TestApiProtoRoundtrip::testOneofExclusivity() {
    pb::ApiMessage msg;
    msg.mutable_client_hello();
    msg.mutable_server_hello();  // replaces client_hello
    QCOMPARE(msg.payload_case(), pb::ApiMessage::kServerHello);
    QVERIFY(!msg.has_client_hello());
}

QTEST_MAIN(TestApiProtoRoundtrip)
#include "test_api_proto_roundtrip.moc"
