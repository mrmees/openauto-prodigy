#include <QtTest>
#include <functional>
#include <limits>
#include "api/api.pb.h"

namespace pb = prodigy::api::v1;

class TestApiProtoRoundtrip : public QObject {
    Q_OBJECT
private slots:
    void testEnvelopeRoundtrip();
    void testOneofExclusivity();
    void testSecurePairingAdditionsRoundtrip();
    void testDataScalarRoundtrip();
    void testDataMetadataPresenceAndCapability();
    void testDataEnvelopeAllocation();
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

void TestApiProtoRoundtrip::testSecurePairingAdditionsRoundtrip() {
    pb::ApiMessage msg;
    auto* challenge = msg.mutable_pairing_challenge();
    challenge->set_nonce("nonce");
    challenge->set_salt("salt");
    challenge->set_secret_format(pb::PAIRING_SECRET_FORMAT_BASE32_120);

    std::string bytes;
    QVERIFY(msg.SerializeToString(&bytes));
    pb::ApiMessage parsed;
    QVERIFY(parsed.ParseFromString(bytes));
    QCOMPARE(parsed.pairing_challenge().secret_format(),
             pb::PAIRING_SECRET_FORMAT_BASE32_120);

    pb::AuthReject reject;
    reject.set_code(pb::AUTH_REJECT_CODE_CREDENTIAL_UPGRADE_REQUIRED);
    QVERIFY(reject.SerializeToString(&bytes));
    pb::AuthReject parsedReject;
    QVERIFY(parsedReject.ParseFromString(bytes));
    QCOMPARE(parsedReject.code(),
             pb::AUTH_REJECT_CODE_CREDENTIAL_UPGRADE_REQUIRED);
}

void TestApiProtoRoundtrip::testDataScalarRoundtrip() {
    pb::DataScalar scalar;
    QCOMPARE(scalar.value_case(), pb::DataScalar::VALUE_NOT_SET);

    std::string bytes;
    scalar.set_double_value(12.5);
    QVERIFY(scalar.SerializeToString(&bytes));
    pb::DataScalar parsed;
    QVERIFY(parsed.ParseFromString(bytes));
    QCOMPARE(parsed.value_case(), pb::DataScalar::kDoubleValue);
    QCOMPARE(parsed.double_value(), 12.5);

    scalar.set_signed_integer_value(std::numeric_limits<qint64>::min());
    QVERIFY(scalar.SerializeToString(&bytes));
    QVERIFY(parsed.ParseFromString(bytes));
    QCOMPARE(parsed.signed_integer_value(), std::numeric_limits<qint64>::min());

    scalar.set_signed_integer_value(std::numeric_limits<qint64>::max());
    QVERIFY(scalar.SerializeToString(&bytes));
    QVERIFY(parsed.ParseFromString(bytes));
    QCOMPARE(parsed.signed_integer_value(), std::numeric_limits<qint64>::max());

    scalar.set_unsigned_integer_value(std::numeric_limits<quint64>::max());
    QVERIFY(scalar.SerializeToString(&bytes));
    QVERIFY(parsed.ParseFromString(bytes));
    QCOMPARE(parsed.unsigned_integer_value(),
             std::numeric_limits<quint64>::max());

    scalar.set_boolean_value(true);
    QVERIFY(scalar.SerializeToString(&bytes));
    QVERIFY(parsed.ParseFromString(bytes));
    QVERIFY(parsed.boolean_value());

    scalar.set_string_value("ready");
    QVERIFY(scalar.SerializeToString(&bytes));
    QVERIFY(parsed.ParseFromString(bytes));
    QCOMPARE(QString::fromStdString(parsed.string_value()), QString("ready"));

    scalar.set_enum_value(-7);
    QVERIFY(scalar.SerializeToString(&bytes));
    QVERIFY(parsed.ParseFromString(bytes));
    QCOMPARE(parsed.value_case(), pb::DataScalar::kEnumValue);
    QCOMPARE(parsed.enum_value(), qint64(-7));

    pb::DataSample absent;
    QVERIFY(!absent.has_value());
    pb::DataSample presentEmpty;
    presentEmpty.mutable_value();
    QVERIFY(presentEmpty.has_value());
    QCOMPARE(presentEmpty.value().value_case(), pb::DataScalar::VALUE_NOT_SET);
}

void TestApiProtoRoundtrip::testDataMetadataPresenceAndCapability() {
    pb::DataChannelDefinition definition;
    definition.set_channel_name("engine.rpm");
    definition.set_value_type(pb::DATA_VALUE_TYPE_DOUBLE);
    QVERIFY(!definition.has_unit());
    QVERIFY(!definition.has_nominal_interval_ms());
    definition.set_unit("");
    definition.set_nominal_interval_ms(0);
    QVERIFY(definition.has_unit());
    QVERIFY(definition.has_nominal_interval_ms());
    QCOMPARE(QString::fromStdString(definition.unit()), QString());
    QCOMPARE(definition.nominal_interval_ms(), quint32(0));

    pb::DataSample sample;
    QVERIFY(!sample.has_observed_at_unix_ms());
    sample.set_observed_at_unix_ms(0);
    QVERIFY(sample.has_observed_at_unix_ms());

    pb::Capabilities capabilities;
    QVERIFY(!capabilities.has_data_provider_bridge());
    capabilities.set_data_provider_bridge(true);
    QVERIFY(capabilities.has_data_provider_bridge());
    QVERIFY(capabilities.data_provider_bridge());
}

void TestApiProtoRoundtrip::testDataEnvelopeAllocation() {
    struct PayloadCase {
        int fieldNumber;
        pb::ApiMessage::PayloadCase expected;
        std::function<void(pb::ApiMessage&)> select;
    };
    const QList<PayloadCase> cases{
        {80, pb::ApiMessage::kRegisterDataProviderRequest,
         [](pb::ApiMessage& m) { m.mutable_register_data_provider_request(); }},
        {81, pb::ApiMessage::kRegisterDataProviderResponse,
         [](pb::ApiMessage& m) { m.mutable_register_data_provider_response(); }},
        {82, pb::ApiMessage::kDeclareDataChannelsRequest,
         [](pb::ApiMessage& m) { m.mutable_declare_data_channels_request(); }},
        {83, pb::ApiMessage::kDeclareDataChannelsResponse,
         [](pb::ApiMessage& m) { m.mutable_declare_data_channels_response(); }},
        {84, pb::ApiMessage::kRemoveDataChannelsRequest,
         [](pb::ApiMessage& m) { m.mutable_remove_data_channels_request(); }},
        {85, pb::ApiMessage::kPublishDataValues,
         [](pb::ApiMessage& m) { m.mutable_publish_data_values(); }},
        {86, pb::ApiMessage::kListDataCatalogRequest,
         [](pb::ApiMessage& m) { m.mutable_list_data_catalog_request(); }},
        {87, pb::ApiMessage::kListDataCatalogResponse,
         [](pb::ApiMessage& m) { m.mutable_list_data_catalog_response(); }},
        {88, pb::ApiMessage::kSubscribeDataChannelsRequest,
         [](pb::ApiMessage& m) { m.mutable_subscribe_data_channels_request(); }},
        {89, pb::ApiMessage::kSubscribeDataChannelsResponse,
         [](pb::ApiMessage& m) { m.mutable_subscribe_data_channels_response(); }},
        {90, pb::ApiMessage::kUnsubscribeDataChannelsRequest,
         [](pb::ApiMessage& m) { m.mutable_unsubscribe_data_channels_request(); }},
        {91, pb::ApiMessage::kDataValuesEvent,
         [](pb::ApiMessage& m) { m.mutable_data_values_event(); }},
        {92, pb::ApiMessage::kWatchDataCatalogRequest,
         [](pb::ApiMessage& m) { m.mutable_watch_data_catalog_request(); }},
        {93, pb::ApiMessage::kDataCatalogEvent,
         [](pb::ApiMessage& m) { m.mutable_data_catalog_event(); }},
        {94, pb::ApiMessage::kDataChannelAvailabilityEvent,
         [](pb::ApiMessage& m) { m.mutable_data_channel_availability_event(); }},
    };

    for (const PayloadCase& c : cases) {
        pb::ApiMessage message;
        c.select(message);
        QCOMPARE(message.payload_case(), c.expected);
        QCOMPARE(message.GetDescriptor()->FindFieldByNumber(c.fieldNumber)->number(),
                 c.fieldNumber);
    }
}

QTEST_MAIN(TestApiProtoRoundtrip)
#include "test_api_proto_roundtrip.moc"
