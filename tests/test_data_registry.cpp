#include <QtTest>

#include "core/services/DataRegistry.hpp"

using namespace oap::data;

namespace {

ProviderDefinition provider(const QString& providerNamespace,
                            const QString& displayName = QStringLiteral("Vehicle")) {
    ProviderDefinition result;
    result.providerNamespace = providerNamespace;
    result.displayName = displayName;
    return result;
}

ChannelDefinition channel(const QString& name, ValueType type,
                          const QString& displayName = QString()) {
    ChannelDefinition result;
    result.channelName = name;
    result.displayName = displayName.isEmpty() ? name : displayName;
    result.valueType = type;
    return result;
}

Sample sample(const QString& name, Scalar value,
              Quality quality = Quality::Good,
              std::optional<qint64> observedAt = std::nullopt) {
    Sample result;
    result.channelName = name;
    result.value = std::move(value);
    result.observedAtUnixMs = observedAt;
    result.quality = quality;
    return result;
}

} // namespace

class TestDataRegistry : public QObject {
    Q_OBJECT
private slots:
    void testProviderOwnershipAndMetadata();
    void testDeclarationsCatalogAndTypeStability();
    void testTypedPublicationAndDuplicateReduction();
    void testRemovalAndOwnerCleanup();
};

void TestDataRegistry::testProviderOwnershipAndMetadata() {
    DataRegistry registry;

    RegistrationResult invalid = registry.registerProvider(1, provider("Bad Namespace"));
    QVERIFY(!invalid.accepted);
    QCOMPARE(registry.catalogRevision(), quint64(0));

    RegistrationResult first =
        registry.registerProvider(1, provider("com.example.vehicle"));
    QVERIFY(first.accepted);
    QCOMPARE(registry.catalogRevision(), quint64(1));
    QVERIFY(registry.providerExists(QStringLiteral("com.example.vehicle")));

    RegistrationResult competing =
        registry.registerProvider(2, provider("com.example.vehicle"));
    QVERIFY(!competing.accepted);
    QCOMPARE(registry.catalogRevision(), quint64(1));

    RegistrationResult repeated =
        registry.registerProvider(1, provider("com.example.vehicle"));
    QVERIFY(repeated.accepted);
    QCOMPARE(registry.catalogRevision(), quint64(1));

    ProviderDefinition changed = provider("com.example.vehicle", "Powertrain");
    changed.description = QStringLiteral("Generic external values");
    QVERIFY(registry.registerProvider(1, changed).accepted);
    QCOMPARE(registry.catalogRevision(), quint64(2));
    QCOMPARE(registry.catalog().providers.first().provider.displayName,
             QStringLiteral("Powertrain"));

    RegistrationResult switched =
        registry.registerProvider(1, provider("com.example.other"));
    QVERIFY(!switched.accepted);
    QCOMPARE(registry.catalogRevision(), quint64(2));

    registry.removeOwner(1);
    QCOMPARE(registry.catalogRevision(), quint64(3));
    QVERIFY(registry.catalog().providers.isEmpty());
    QVERIFY(registry.registerProvider(2, provider("com.example.vehicle")).accepted);
}

void TestDataRegistry::testDeclarationsCatalogAndTypeStability() {
    DataRegistry registry;
    QList<QPair<ChannelRef, bool>> availability;
    connect(&registry, &DataRegistry::availabilityChanged, this,
            [&availability](const ChannelRef& ref, bool available,
                            UnavailableReason, quint64) {
                availability.append({ref, available});
            });

    const QList<DeclarationResult> before = registry.declareChannels(
        9, {channel("engine.rpm", ValueType::Double)});
    QCOMPARE(before.size(), 1);
    QVERIFY(!before.first().accepted);
    QCOMPARE(before.first().reason, QStringLiteral("provider not registered"));

    QVERIFY(registry.registerProvider(9, provider("com.example.vehicle")).accepted);
    const QList<DeclarationResult> partial = registry.declareChannels(
        9,
        {channel("Bad Channel", ValueType::Double),
         channel("engine.unknown", ValueType::Unspecified),
         channel("engine.rpm", ValueType::Double, "Engine speed")});
    QCOMPARE(partial.size(), 3);
    QVERIFY(!partial[0].accepted);
    QVERIFY(!partial[1].accepted);
    QVERIFY(partial[2].accepted);
    QCOMPARE(registry.catalogRevision(), quint64(2));
    QCOMPARE(availability.size(), 1);

    Catalog catalog = registry.catalog();
    QCOMPARE(catalog.providers.size(), 1);
    QCOMPARE(catalog.providers.first().channels.size(), 1);
    QCOMPARE(catalog.providers.first().channels.first().channelName,
             QStringLiteral("engine.rpm"));

    QVERIFY(registry.declareChannels(
        9, {channel("engine.rpm", ValueType::Double, "Engine speed")})
                .first().accepted);
    QCOMPARE(registry.catalogRevision(), quint64(2));
    QCOMPARE(availability.size(), 1);

    ChannelDefinition updated =
        channel("engine.rpm", ValueType::Double, "Engine RPM");
    updated.unit = QStringLiteral("rpm");
    updated.staleAfterMs = 1500;
    QVERIFY(registry.declareChannels(9, {updated}).first().accepted);
    QCOMPARE(registry.catalogRevision(), quint64(3));
    QCOMPARE(availability.size(), 2);

    DeclarationResult typeChange = registry.declareChannels(
        9, {channel("engine.rpm", ValueType::UnsignedInteger)})
                                       .first();
    QVERIFY(!typeChange.accepted);
    QCOMPARE(registry.catalogRevision(), quint64(3));
    QCOMPARE(registry.definition(
                 {QStringLiteral("com.example.vehicle"),
                  QStringLiteral("engine.rpm")})
                 ->valueType,
             ValueType::Double);

    QVERIFY(registry.declareChannels(
        9,
        {channel("z.last", ValueType::Boolean),
         channel("a.first", ValueType::String)})
                .first().accepted);
    catalog = registry.catalog();
    QCOMPARE(catalog.providers.first().channels[0].channelName,
             QStringLiteral("a.first"));
    QCOMPARE(catalog.providers.first().channels[1].channelName,
             QStringLiteral("engine.rpm"));
    QCOMPARE(catalog.providers.first().channels[2].channelName,
             QStringLiteral("z.last"));
}

void TestDataRegistry::testTypedPublicationAndDuplicateReduction() {
    DataRegistry registry;
    registry.setNowUnixMsForTest([] { return qint64(424242); });
    QVERIFY(registry.registerProvider(7, provider("com.example.vehicle")).accepted);
    registry.declareChannels(
        7,
        {channel("value.double", ValueType::Double),
         channel("value.signed", ValueType::SignedInteger),
         channel("value.unsigned", ValueType::UnsignedInteger),
         channel("value.boolean", ValueType::Boolean),
         channel("value.string", ValueType::String),
         channel("value.enum", ValueType::Enum)});

    PublishResult allTypes = registry.publish(
        7,
        {sample("value.double", 1.5),
         sample("value.signed", qint64(-2)),
         sample("value.unsigned", quint64(3)),
         sample("value.boolean", true),
         sample("value.string", QStringLiteral("ready")),
         sample("value.enum", EnumScalar{-4})});
    QCOMPARE(allTypes.acceptedSamples.size(), 6);
    QVERIFY(allTypes.diagnostics.isEmpty());
    for (const Sample& accepted : allTypes.acceptedSamples) {
        QVERIFY(accepted.observedAtUnixMs.has_value());
        QCOMPARE(*accepted.observedAtUnixMs, qint64(424242));
    }

    PublishResult mixed = registry.publish(
        7,
        {sample("value.double", 10.0),
         sample("value.string", QStringLiteral("first")),
         sample("value.double", 11.0),
         sample("missing.channel", 1.0)});
    QCOMPARE(mixed.acceptedSamples.size(), 2);
    QCOMPARE(mixed.acceptedSamples[0].channelName, QStringLiteral("value.string"));
    QCOMPARE(mixed.acceptedSamples[1].channelName, QStringLiteral("value.double"));
    QCOMPARE(std::get<double>(*mixed.acceptedSamples[1].value), 11.0);
    QCOMPARE(mixed.diagnostics.size(), 1);

    PublishResult invalidWinner = registry.publish(
        7,
        {sample("value.double", 20.0),
         sample("value.boolean", false),
         sample("value.double", QStringLiteral("wrong type"))});
    QCOMPARE(invalidWinner.acceptedSamples.size(), 1);
    QCOMPARE(invalidWinner.acceptedSamples.first().channelName,
             QStringLiteral("value.boolean"));
    QCOMPARE(std::get<double>(*registry.latestSample(
                                  {QStringLiteral("com.example.vehicle"),
                                   QStringLiteral("value.double")})
                                  ->value),
             11.0);

    Sample missingValue;
    missingValue.channelName = QStringLiteral("value.double");
    missingValue.quality = Quality::Good;
    QVERIFY(registry.publish(7, {missingValue}).acceptedSamples.isEmpty());

    missingValue.quality = Quality::Unavailable;
    PublishResult unavailable = registry.publish(7, {missingValue});
    QCOMPARE(unavailable.acceptedSamples.size(), 1);
    QVERIFY(!unavailable.acceptedSamples.first().value.has_value());

    PublishResult explicitTimestamp =
        registry.publish(7, {sample("value.signed", qint64(8), Quality::Good, 99)});
    QCOMPARE(*explicitTimestamp.acceptedSamples.first().observedAtUnixMs, qint64(99));
}

void TestDataRegistry::testRemovalAndOwnerCleanup() {
    DataRegistry registry;
    QVERIFY(registry.registerProvider(4, provider("com.example.vehicle")).accepted);
    registry.declareChannels(
        4,
        {channel("engine.rpm", ValueType::Double),
         channel("engine.load", ValueType::Double)});
    registry.publish(4, {sample("engine.rpm", 900.0), sample("engine.load", 0.2)});

    QList<QPair<QString, UnavailableReason>> unavailable;
    connect(&registry, &DataRegistry::availabilityChanged, this,
            [&unavailable](const ChannelRef& ref, bool available,
                           UnavailableReason reason, quint64) {
                if (!available) unavailable.append({ref.channelName, reason});
            });

    const quint64 beforeUnknown = registry.catalogRevision();
    registry.removeChannels(4, {QStringLiteral("missing")});
    QCOMPARE(registry.catalogRevision(), beforeUnknown);

    registry.removeChannels(4, {QStringLiteral("engine.rpm")});
    QCOMPARE(registry.catalogRevision(), beforeUnknown + 1);
    QVERIFY(!registry.latestSample({QStringLiteral("com.example.vehicle"),
                                    QStringLiteral("engine.rpm")})
                 .has_value());
    QCOMPARE(unavailable.size(), 1);
    QCOMPARE(unavailable.first().second, UnavailableReason::ChannelRemoved);

    registry.removeOwner(4);
    QCOMPARE(registry.catalogRevision(), beforeUnknown + 2);
    QCOMPARE(unavailable.size(), 2);
    QCOMPARE(unavailable.last().first, QStringLiteral("engine.load"));
    QCOMPARE(unavailable.last().second, UnavailableReason::ProviderDisconnected);
    QVERIFY(registry.catalog().providers.isEmpty());

    registry.removeOwner(4);
    QCOMPARE(registry.catalogRevision(), beforeUnknown + 2);
}

QTEST_MAIN(TestDataRegistry)
#include "test_data_registry.moc"
