#include <QTest>

#include <oaa/Session/SessionProtocolPolicy.hpp>

#include <type_traits>
#include <utility>

namespace {

template <typename T, typename = void>
struct HasReportedVersion : std::false_type {};

template <typename T>
struct HasReportedVersion<
    T,
    std::void_t<decltype(std::declval<const T&>().reportedVersion())>>
    : std::true_type {};

static_assert(!HasReportedVersion<oaa::SessionProtocolPolicy>::value,
              "Phone-reported GAL must not become a local policy input");

} // namespace

class TestSessionProtocolPolicy : public QObject {
    Q_OBJECT

private slots:
    void protocolVersionsUseNumericPairOrdering()
    {
        constexpr oaa::ProtocolVersion oneSeven{1, 7};
        constexpr oaa::ProtocolVersion oneTen{1, 10};
        constexpr oaa::ProtocolVersion fourThree{4, 3};

        QVERIFY(oneSeven < oneTen);
        QVERIFY(oneTen < fourThree);
        QVERIFY(fourThree >= oneTen);
        QVERIFY((fourThree == oaa::ProtocolVersion{4, 3}));
        QVERIFY(fourThree != oneSeven);
    }

    void requestedVersionActivatesOnlyReachedThresholds_data()
    {
        QTest::addColumn<int>("major");
        QTest::addColumn<int>("minor");
        QTest::addColumn<bool>("minimumResponse");
        QTest::addColumn<bool>("modernDisplay");
        QTest::addColumn<bool>("acklessAudio");
        QTest::addColumn<bool>("singleVideoCodec");
        QTest::addColumn<bool>("audioMediaOptions");
        QTest::addColumn<bool>("vehicleEnergyForecast");
        QTest::addColumn<bool>("videoMediaOptions");

        QTest::newRow("1.7")
            << 1 << 7 << false << false << false << false << false << false
            << false;
        QTest::newRow("4.3")
            << 4 << 3 << true << true << false << false << false << false
            << false;
        QTest::newRow("5.0")
            << 5 << 0 << true << true << true << true << false << false
            << false;
        QTest::newRow("5.1")
            << 5 << 1 << true << true << true << true << true << true
            << false;
        QTest::newRow("6.0")
            << 6 << 0 << true << true << true << true << true << true
            << true;
    }

    void requestedVersionActivatesOnlyReachedThresholds()
    {
        QFETCH(int, major);
        QFETCH(int, minor);
        QFETCH(bool, minimumResponse);
        QFETCH(bool, modernDisplay);
        QFETCH(bool, acklessAudio);
        QFETCH(bool, singleVideoCodec);
        QFETCH(bool, audioMediaOptions);
        QFETCH(bool, vehicleEnergyForecast);
        QFETCH(bool, videoMediaOptions);

        const oaa::ProtocolVersion version{
            static_cast<uint16_t>(major), static_cast<uint16_t>(minor)};
        const oaa::SessionProtocolPolicy policy(version);

        QCOMPARE(policy.requestedVersion(), version);
        QCOMPARE(policy.atLeast(version), true);
        QCOMPARE(policy.requiresMinimumCompatibleResponse(), minimumResponse);
        QCOMPARE(policy.usesModernDisplayPolicy(), modernDisplay);
        QCOMPARE(policy.usesAcklessAudio(), acklessAudio);
        QCOMPARE(policy.requiresSingleVideoCodecPerDisplay(), singleVideoCodec);
        QCOMPARE(policy.acceptsAudioMediaOptions(), audioMediaOptions);
        QCOMPARE(policy.acceptsVehicleEnergyForecast(), vehicleEnergyForecast);
        QCOMPARE(policy.acceptsVideoMediaOptions(), videoMediaOptions);
    }
};

QTEST_MAIN(TestSessionProtocolPolicy)
#include "test_session_protocol_policy.moc"
