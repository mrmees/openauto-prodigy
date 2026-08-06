#include <QtTest>
#include <QHostAddress>
#include <QSignalSpy>

#include "api/api.pb.h"
#include "core/api/ApiRequestHandlers.hpp"
#include "core/api/ApiSession.hpp"
#include "core/api/ApiTransport.hpp"
#include "core/services/DataRegistry.hpp"

namespace pb = prodigy::api::v1;
using oap::api::ApiRequestHandlers;
using oap::api::ApiSession;
using oap::api::ApiSessionDeps;
using oap::api::IApiTransport;
using oap::data::ChannelRef;
using oap::data::DataRegistry;

class FakeTransport final : public IApiTransport {
    Q_OBJECT
public:
    QList<QByteArray> sent;
    qint64 fakeBytesToWrite = 0;

    void sendMessage(const QByteArray& serialized) override { sent.append(serialized); }
    qint64 bytesToWrite() const override { return fakeBytesToWrite; }
    void close() override { emit closed(); }
    void abort() override { emit closed(); }
    QHostAddress peerAddress() const override {
        return QHostAddress(QHostAddress::LocalHost);
    }

    void inject(const pb::ApiMessage& message) {
        std::string bytes;
        message.SerializeToString(&bytes);
        emit messageReceived(QByteArray::fromStdString(bytes));
    }
};

namespace {

pb::ApiMessage parse(const QByteArray& bytes) {
    pb::ApiMessage message;
    const bool parsed = message.ParseFromArray(bytes.constData(), bytes.size());
    Q_ASSERT(parsed);
    return message;
}

void ready(FakeTransport* transport) {
    pb::ApiMessage hello;
    hello.set_request_id(1);
    auto* payload = hello.mutable_client_hello();
    payload->set_requested_api_version_major(1);
    payload->set_client_name("data test");
    payload->set_client_kind(pb::CLIENT_KIND_DIAGNOSTIC);
    transport->inject(hello);
}

pb::ApiMessage registration(quint64 requestId,
                            const char* displayName = "Vehicle") {
    pb::ApiMessage message;
    message.set_request_id(requestId);
    auto* provider = message.mutable_register_data_provider_request()
                         ->mutable_provider();
    provider->set_provider_namespace("com.example.vehicle");
    provider->set_display_name(displayName);
    return message;
}

pb::ApiMessage declaration(quint64 requestId) {
    pb::ApiMessage message;
    message.set_request_id(requestId);
    auto* request = message.mutable_declare_data_channels_request();
    auto* rpm = request->add_channels();
    rpm->set_channel_name("engine.rpm");
    rpm->set_display_name("Engine RPM");
    rpm->set_value_type(pb::DATA_VALUE_TYPE_DOUBLE);
    rpm->set_unit("rpm");
    auto* invalid = request->add_channels();
    invalid->set_channel_name("Bad Channel");
    invalid->set_value_type(pb::DATA_VALUE_TYPE_DOUBLE);
    return message;
}

pb::ApiMessage subscription(quint64 requestId, const char* providerNamespace,
                            const char* channelName) {
    pb::ApiMessage message;
    message.set_request_id(requestId);
    auto* ref = message.mutable_subscribe_data_channels_request()->add_channels();
    ref->set_provider_namespace(providerNamespace);
    ref->set_channel_name(channelName);
    return message;
}

pb::ApiMessage rpmPublication(double value) {
    pb::ApiMessage message;
    auto* sample = message.mutable_publish_data_values()->add_samples();
    sample->set_channel_name("engine.rpm");
    sample->mutable_value()->set_double_value(value);
    sample->set_quality(pb::DATA_QUALITY_GOOD);
    return message;
}

} // namespace

class TestApiDataBridge : public QObject {
    Q_OBJECT
private slots:
    void testProviderCommandsPublicationAndCleanup();
    void testCatalogListAndWatchOrdering();
    void testZeroRequestIdClosesResponseBearingRequests();
    void testWaitingSubscriptionLifecycleAndSnapshots();
    void testExactFilteringAndDuplicateNormalization();
    void testUnsubscribeAndSlowConsumerIsolation();
};

void TestApiDataBridge::testProviderCommandsPublicationAndCleanup() {
    DataRegistry registry;
    ApiRequestHandlers handler({nullptr, nullptr, nullptr, nullptr, &registry});
    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.requests = &handler;
    ApiSession session(transport, deps);
    ready(transport);
    transport->sent.clear();

    transport->inject(registration(10));
    pb::ApiMessage response = parse(transport->sent.takeLast());
    QCOMPARE(response.request_id(), quint64(10));
    QCOMPARE(response.payload_case(),
             pb::ApiMessage::kRegisterDataProviderResponse);
    QVERIFY(response.register_data_provider_response().accepted());
    QCOMPARE(registry.catalogRevision(), quint64(1));

    transport->inject(registration(11));
    QVERIFY(parse(transport->sent.takeLast())
                .register_data_provider_response().accepted());
    QCOMPARE(registry.catalogRevision(), quint64(1));

    pb::ApiMessage switched = registration(12);
    switched.mutable_register_data_provider_request()
        ->mutable_provider()->set_provider_namespace("com.example.other");
    transport->inject(switched);
    QVERIFY(!parse(transport->sent.takeLast())
                 .register_data_provider_response().accepted());
    QCOMPARE(registry.catalogRevision(), quint64(1));

    transport->inject(declaration(13));
    response = parse(transport->sent.takeLast());
    QCOMPARE(response.payload_case(),
             pb::ApiMessage::kDeclareDataChannelsResponse);
    QCOMPARE(response.declare_data_channels_response().results_size(), 2);
    QVERIFY(response.declare_data_channels_response().results(0).accepted());
    QVERIFY(!response.declare_data_channels_response().results(1).accepted());

    const int beforePublish = transport->sent.size();
    pb::ApiMessage publish;
    auto* sample = publish.mutable_publish_data_values()->add_samples();
    sample->set_channel_name("engine.rpm");
    sample->mutable_value()->set_double_value(975.5);
    sample->set_quality(pb::DATA_QUALITY_GOOD);
    transport->inject(publish);
    QCOMPARE(transport->sent.size(), beforePublish);
    const auto retained = registry.latestSample(
        {QStringLiteral("com.example.vehicle"), QStringLiteral("engine.rpm")});
    QVERIFY(retained.has_value());
    QCOMPARE(std::get<double>(*retained->value), 975.5);

    pb::ApiMessage malformedWinner;
    auto* earlier = malformedWinner.mutable_publish_data_values()->add_samples();
    earlier->set_channel_name("engine.rpm");
    earlier->mutable_value()->set_double_value(1500.0);
    earlier->set_quality(pb::DATA_QUALITY_GOOD);
    auto* winner = malformedWinner.mutable_publish_data_values()->add_samples();
    winner->set_channel_name("engine.rpm");
    winner->mutable_value()->set_double_value(1600.0);
    winner->set_quality(static_cast<pb::DataQuality>(99));
    transport->inject(malformedWinner);
    QCOMPARE(std::get<double>(*registry.latestSample(
                                  {QStringLiteral("com.example.vehicle"),
                                   QStringLiteral("engine.rpm")})
                                  ->value),
             975.5);

    publish.set_request_id(99);
    publish.mutable_publish_data_values()->mutable_samples(0)
        ->mutable_value()->set_double_value(2000.0);
    transport->inject(publish);
    QCOMPARE(transport->sent.size(), beforePublish);
    QCOMPARE(std::get<double>(*registry.latestSample(
                                  {QStringLiteral("com.example.vehicle"),
                                   QStringLiteral("engine.rpm")})
                                  ->value),
             975.5);
    QCOMPARE(session.state(), ApiSession::State::Ready);

    pb::ApiMessage remove;
    remove.set_request_id(14);
    remove.mutable_remove_data_channels_request()->add_channel_names("engine.rpm");
    transport->inject(remove);
    QCOMPARE(parse(transport->sent.takeLast()).payload_case(),
             pb::ApiMessage::kAck);
    QVERIFY(!registry.definition(
                 {QStringLiteral("com.example.vehicle"),
                  QStringLiteral("engine.rpm")})
                 .has_value());

    transport->close();
    QCOMPARE(session.state(), ApiSession::State::Closed);
    QVERIFY(registry.catalog().providers.isEmpty());
}

void TestApiDataBridge::testCatalogListAndWatchOrdering() {
    DataRegistry registry;
    ApiRequestHandlers handler({nullptr, nullptr, nullptr, nullptr, &registry});

    auto* providerTransport = new FakeTransport();
    ApiSessionDeps providerDeps;
    providerDeps.requests = &handler;
    ApiSession providerSession(providerTransport, providerDeps);
    ready(providerTransport);
    providerTransport->sent.clear();
    providerTransport->inject(registration(10));
    providerTransport->inject(declaration(11));

    auto* consumerTransport = new FakeTransport();
    ApiSessionDeps consumerDeps;
    consumerDeps.requests = &handler;
    ApiSession consumerSession(consumerTransport, consumerDeps);
    ready(consumerTransport);
    consumerTransport->sent.clear();

    pb::ApiMessage list;
    list.set_request_id(20);
    list.mutable_list_data_catalog_request();
    consumerTransport->inject(list);
    pb::ApiMessage listed = parse(consumerTransport->sent.takeLast());
    QCOMPARE(listed.request_id(), quint64(20));
    QCOMPARE(listed.payload_case(), pb::ApiMessage::kListDataCatalogResponse);
    QCOMPARE(listed.list_data_catalog_response().catalog().providers_size(), 1);
    QCOMPARE(listed.list_data_catalog_response().catalog()
                 .providers(0).channels_size(), 1);

    pb::ApiMessage watch;
    watch.set_request_id(21);
    watch.mutable_watch_data_catalog_request()->set_enabled(true);
    consumerTransport->inject(watch);
    QCOMPARE(consumerTransport->sent.size(), 2);
    QCOMPARE(parse(consumerTransport->sent[0]).payload_case(),
             pb::ApiMessage::kAck);
    const pb::ApiMessage initial = parse(consumerTransport->sent[1]);
    QCOMPARE(initial.request_id(), quint64(0));
    QCOMPARE(initial.payload_case(), pb::ApiMessage::kDataCatalogEvent);
    consumerTransport->sent.clear();

    providerTransport->inject(registration(12, "Updated Vehicle"));
    QCOMPARE(consumerTransport->sent.size(), 1);
    QCOMPARE(parse(consumerTransport->sent.first()).data_catalog_event()
                 .catalog().providers(0).provider().display_name(),
             std::string("Updated Vehicle"));
    consumerTransport->sent.clear();

    providerTransport->inject(registration(13, "Updated Vehicle"));
    QVERIFY(consumerTransport->sent.isEmpty());

    watch.set_request_id(22);
    watch.mutable_watch_data_catalog_request()->set_enabled(false);
    consumerTransport->inject(watch);
    QCOMPARE(consumerTransport->sent.size(), 1);
    QCOMPARE(parse(consumerTransport->sent.takeLast()).payload_case(),
             pb::ApiMessage::kAck);

    providerTransport->inject(registration(14, "No Watch Event"));
    QVERIFY(consumerTransport->sent.isEmpty());
}

void TestApiDataBridge::testZeroRequestIdClosesResponseBearingRequests() {
    enum class Request { Register, Declare, Remove, List, Watch, Subscribe, Unsubscribe };
    const QList<Request> requests = {
        Request::Register, Request::Declare, Request::Remove,
        Request::List, Request::Watch, Request::Subscribe, Request::Unsubscribe,
    };

    for (Request request : requests) {
        DataRegistry registry;
        ApiRequestHandlers handler({nullptr, nullptr, nullptr, nullptr, &registry});
        auto* transport = new FakeTransport();
        ApiSessionDeps deps;
        deps.requests = &handler;
        ApiSession session(transport, deps);
        ready(transport);
        transport->sent.clear();

        pb::ApiMessage message;
        switch (request) {
        case Request::Register:
            message = registration(0);
            break;
        case Request::Declare:
            message = declaration(0);
            break;
        case Request::Remove:
            message.mutable_remove_data_channels_request();
            break;
        case Request::List:
            message.mutable_list_data_catalog_request();
            break;
        case Request::Watch:
            message.mutable_watch_data_catalog_request()->set_enabled(true);
            break;
        case Request::Subscribe:
            message = subscription(0, "com.example.vehicle", "engine.rpm");
            break;
        case Request::Unsubscribe:
            message.mutable_unsubscribe_data_channels_request()->add_channels()
                ->set_provider_namespace("com.example.vehicle");
            break;
        }
        transport->inject(message);

        QCOMPARE(session.state(), ApiSession::State::Closed);
        QVERIFY(!transport->sent.isEmpty());
        const pb::ApiMessage error = parse(transport->sent.first());
        QCOMPARE(error.request_id(), quint64(0));
        QCOMPARE(error.payload_case(), pb::ApiMessage::kError);
        QCOMPARE(error.error().code(), pb::ERROR_CODE_INVALID_REQUEST);
    }
}

void TestApiDataBridge::testWaitingSubscriptionLifecycleAndSnapshots() {
    DataRegistry registry;
    ApiRequestHandlers handler({nullptr, nullptr, nullptr, nullptr, &registry});

    auto* consumerTransport = new FakeTransport();
    ApiSessionDeps consumerDeps;
    consumerDeps.requests = &handler;
    ApiSession consumer(consumerTransport, consumerDeps);
    ready(consumerTransport);
    consumerTransport->sent.clear();

    consumerTransport->inject(
        subscription(30, "com.example.vehicle", "engine.rpm"));
    QCOMPARE(consumerTransport->sent.size(), 2);
    QCOMPARE(parse(consumerTransport->sent[0]).payload_case(),
             pb::ApiMessage::kSubscribeDataChannelsResponse);
    const pb::ApiMessage absent = parse(consumerTransport->sent[1]);
    QCOMPARE(absent.payload_case(),
             pb::ApiMessage::kDataChannelAvailabilityEvent);
    QCOMPARE(absent.data_channel_availability_event().unavailable_reason(),
             pb::DATA_UNAVAILABLE_REASON_PROVIDER_ABSENT);
    consumerTransport->sent.clear();

    auto* providerTransport = new FakeTransport();
    ApiSessionDeps providerDeps;
    providerDeps.requests = &handler;
    ApiSession provider(providerTransport, providerDeps);
    ready(providerTransport);
    providerTransport->sent.clear();
    providerTransport->inject(registration(31));
    QCOMPARE(consumerTransport->sent.size(), 1);
    QCOMPARE(parse(consumerTransport->sent.takeLast())
                 .data_channel_availability_event().unavailable_reason(),
             pb::DATA_UNAVAILABLE_REASON_CHANNEL_ABSENT);

    providerTransport->inject(declaration(32));
    QCOMPARE(consumerTransport->sent.size(), 1);
    pb::ApiMessage available = parse(consumerTransport->sent.takeLast());
    QCOMPARE(available.data_channel_availability_event().availability(),
             pb::DATA_CHANNEL_AVAILABILITY_AVAILABLE);
    QCOMPARE(available.data_channel_availability_event().definition().unit(),
             std::string("rpm"));

    providerTransport->inject(rpmPublication(1100.0));
    QCOMPARE(consumerTransport->sent.size(), 1);
    QCOMPARE(parse(consumerTransport->sent.takeLast())
                 .data_values_event().samples(0).value().double_value(),
             1100.0);

    consumerTransport->inject(
        subscription(33, "com.example.vehicle", "engine.rpm"));
    QCOMPARE(consumerTransport->sent.size(), 3);
    QCOMPARE(parse(consumerTransport->sent[0]).payload_case(),
             pb::ApiMessage::kSubscribeDataChannelsResponse);
    QCOMPARE(parse(consumerTransport->sent[1]).payload_case(),
             pb::ApiMessage::kDataChannelAvailabilityEvent);
    QCOMPARE(parse(consumerTransport->sent[2]).payload_case(),
             pb::ApiMessage::kDataValuesEvent);
    consumerTransport->sent.clear();

    pb::ApiMessage metadata = declaration(34);
    metadata.mutable_declare_data_channels_request()->mutable_channels(0)
        ->set_display_name("Tachometer input");
    providerTransport->inject(metadata);
    QCOMPARE(consumerTransport->sent.size(), 1);
    QCOMPARE(parse(consumerTransport->sent.takeLast())
                 .data_channel_availability_event().definition().display_name(),
             std::string("Tachometer input"));

    pb::ApiMessage remove;
    remove.set_request_id(35);
    remove.mutable_remove_data_channels_request()->add_channel_names("engine.rpm");
    providerTransport->inject(remove);
    QCOMPARE(parse(consumerTransport->sent.takeLast())
                 .data_channel_availability_event().unavailable_reason(),
             pb::DATA_UNAVAILABLE_REASON_CHANNEL_REMOVED);

    providerTransport->inject(declaration(36));
    QCOMPARE(parse(consumerTransport->sent.takeLast())
                 .data_channel_availability_event().availability(),
             pb::DATA_CHANNEL_AVAILABILITY_AVAILABLE);
    providerTransport->close();
    QCOMPARE(parse(consumerTransport->sent.takeLast())
                 .data_channel_availability_event().unavailable_reason(),
             pb::DATA_UNAVAILABLE_REASON_PROVIDER_DISCONNECTED);

    auto* replacementTransport = new FakeTransport();
    ApiSessionDeps replacementDeps;
    replacementDeps.requests = &handler;
    ApiSession replacement(replacementTransport, replacementDeps);
    ready(replacementTransport);
    replacementTransport->sent.clear();
    replacementTransport->inject(registration(37));
    QCOMPARE(parse(consumerTransport->sent.takeLast())
                 .data_channel_availability_event().unavailable_reason(),
             pb::DATA_UNAVAILABLE_REASON_CHANNEL_ABSENT);
    replacementTransport->inject(declaration(38));
    QCOMPARE(parse(consumerTransport->sent.takeLast())
                 .data_channel_availability_event().availability(),
             pb::DATA_CHANNEL_AVAILABILITY_AVAILABLE);
}

void TestApiDataBridge::testExactFilteringAndDuplicateNormalization() {
    DataRegistry registry;
    ApiRequestHandlers handler({nullptr, nullptr, nullptr, nullptr, &registry});
    auto* providerTransport = new FakeTransport();
    ApiSessionDeps providerDeps;
    providerDeps.requests = &handler;
    ApiSession provider(providerTransport, providerDeps);
    ready(providerTransport);
    providerTransport->sent.clear();
    providerTransport->inject(registration(40));
    providerTransport->inject(declaration(41));

    pb::ApiMessage loadDeclaration;
    loadDeclaration.set_request_id(42);
    auto* load = loadDeclaration.mutable_declare_data_channels_request()->add_channels();
    load->set_channel_name("engine.load");
    load->set_display_name("Engine load");
    load->set_value_type(pb::DATA_VALUE_TYPE_DOUBLE);
    providerTransport->inject(loadDeclaration);

    auto* rpmTransport = new FakeTransport();
    ApiSessionDeps rpmDeps;
    rpmDeps.requests = &handler;
    ApiSession rpmConsumer(rpmTransport, rpmDeps);
    ready(rpmTransport);
    rpmTransport->sent.clear();
    rpmTransport->inject(subscription(43, "com.example.vehicle", "engine.rpm"));
    rpmTransport->sent.clear();

    auto* loadTransport = new FakeTransport();
    ApiSessionDeps loadDeps;
    loadDeps.requests = &handler;
    ApiSession loadConsumer(loadTransport, loadDeps);
    ready(loadTransport);
    loadTransport->sent.clear();
    loadTransport->inject(subscription(44, "com.example.vehicle", "engine.load"));
    loadTransport->sent.clear();

    pb::ApiMessage batch;
    auto* rpm1 = batch.mutable_publish_data_values()->add_samples();
    rpm1->set_channel_name("engine.rpm");
    rpm1->mutable_value()->set_double_value(1000.0);
    rpm1->set_quality(pb::DATA_QUALITY_GOOD);
    auto* load1 = batch.mutable_publish_data_values()->add_samples();
    load1->set_channel_name("engine.load");
    load1->mutable_value()->set_double_value(0.25);
    load1->set_quality(pb::DATA_QUALITY_GOOD);
    auto* rpm2 = batch.mutable_publish_data_values()->add_samples();
    rpm2->set_channel_name("engine.rpm");
    rpm2->mutable_value()->set_double_value(1250.0);
    rpm2->set_quality(pb::DATA_QUALITY_GOOD);
    providerTransport->inject(batch);

    QCOMPARE(rpmTransport->sent.size(), 1);
    const pb::ApiMessage rpmMessage = parse(rpmTransport->sent.first());
    const auto& rpmEvent = rpmMessage.data_values_event();
    QCOMPARE(rpmEvent.samples_size(), 1);
    QCOMPARE(rpmEvent.samples(0).channel_name(), std::string("engine.rpm"));
    QCOMPARE(rpmEvent.samples(0).value().double_value(), 1250.0);
    QCOMPARE(loadTransport->sent.size(), 1);
    const pb::ApiMessage loadMessage = parse(loadTransport->sent.first());
    const auto& loadEvent = loadMessage.data_values_event();
    QCOMPARE(loadEvent.samples_size(), 1);
    QCOMPARE(loadEvent.samples(0).channel_name(), std::string("engine.load"));
    QCOMPARE(loadEvent.samples(0).value().double_value(), 0.25);
}

void TestApiDataBridge::testUnsubscribeAndSlowConsumerIsolation() {
    DataRegistry registry;
    ApiRequestHandlers handler({nullptr, nullptr, nullptr, nullptr, &registry});
    auto* providerTransport = new FakeTransport();
    ApiSessionDeps providerDeps;
    providerDeps.requests = &handler;
    ApiSession provider(providerTransport, providerDeps);
    ready(providerTransport);
    providerTransport->sent.clear();
    providerTransport->inject(registration(50));
    providerTransport->inject(declaration(51));

    auto* slowTransport = new FakeTransport();
    ApiSessionDeps slowDeps;
    slowDeps.requests = &handler;
    slowDeps.maxQueueBytes = 128;
    ApiSession slow(slowTransport, slowDeps);
    ready(slowTransport);
    slowTransport->sent.clear();
    slowTransport->inject(subscription(52, "com.example.vehicle", "engine.rpm"));
    slowTransport->sent.clear();

    auto* fastTransport = new FakeTransport();
    ApiSessionDeps fastDeps;
    fastDeps.requests = &handler;
    ApiSession fast(fastTransport, fastDeps);
    ready(fastTransport);
    fastTransport->sent.clear();
    fastTransport->inject(subscription(53, "com.example.vehicle", "engine.rpm"));
    fastTransport->sent.clear();

    slowTransport->fakeBytesToWrite = 128;
    providerTransport->inject(rpmPublication(900.0));
    QCOMPARE(slow.state(), ApiSession::State::Closed);
    QCOMPARE(fast.state(), ApiSession::State::Ready);
    QCOMPARE(fastTransport->sent.size(), 1);
    QCOMPARE(parse(fastTransport->sent.takeLast())
                 .data_values_event().samples(0).value().double_value(),
             900.0);
    QVERIFY(registry.providerExists(QStringLiteral("com.example.vehicle")));

    pb::ApiMessage unsubscribe;
    unsubscribe.set_request_id(54);
    auto* ref = unsubscribe.mutable_unsubscribe_data_channels_request()->add_channels();
    ref->set_provider_namespace("com.example.vehicle");
    ref->set_channel_name("engine.rpm");
    fastTransport->inject(unsubscribe);
    QCOMPARE(parse(fastTransport->sent.takeLast()).payload_case(),
             pb::ApiMessage::kAck);
    fastTransport->inject(unsubscribe);
    QCOMPARE(parse(fastTransport->sent.takeLast()).payload_case(),
             pb::ApiMessage::kAck);

    providerTransport->inject(rpmPublication(950.0));
    QVERIFY(fastTransport->sent.isEmpty());
}

QTEST_MAIN(TestApiDataBridge)
#include "test_api_data_bridge.moc"
