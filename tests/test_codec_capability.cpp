#include <QtTest>
#include "core/aa/CodecCapability.hpp"

using namespace oap::aa;

class TestCodecCapability : public QObject
{
    Q_OBJECT

private slots:
    void probeFindsH264Software()
    {
        auto caps = CodecCapability::probe();

        // H.264 software decoder ("h264") should always be available in any
        // FFmpeg build that has libavcodec.
        QVERIFY(caps.count("h264") > 0);
        QVERIFY(!caps["h264"].software.empty());

        // Verify the software decoder name is "h264"
        bool foundH264 = false;
        for (const auto& dec : caps["h264"].software) {
            if (dec.name == "h264") {
                foundH264 = true;
                QVERIFY(!dec.isHardware);
            }
        }
        QVERIFY(foundH264);
    }

    void resultStructureIsWellFormed()
    {
        auto caps = CodecCapability::probe();

        for (const auto& [codecName, info] : caps) {
            // Codec name must be non-empty
            QVERIFY(!codecName.empty());

            // Must have at least one decoder (otherwise it wouldn't be in the map)
            QVERIFY(!info.hardware.empty() || !info.software.empty());

            // All decoder entries must have non-empty names
            for (const auto& dec : info.hardware) {
                QVERIFY(!dec.name.empty());
                QVERIFY(dec.isHardware);
            }
            for (const auto& dec : info.software) {
                QVERIFY(!dec.name.empty());
                QVERIFY(!dec.isHardware);
            }
        }
    }

    void availableCodecsIncludesH264()
    {
        auto caps = CodecCapability::probe();
        auto codecs = CodecCapability::availableCodecs(caps);

        bool hasH264 = false;
        for (const auto& c : codecs) {
            if (c == "h264") {
                hasH264 = true;
                break;
            }
        }
        QVERIFY(hasH264);
    }

    void availableCodecsEmptyForEmptyMap()
    {
        std::map<std::string, CodecInfo> empty;
        auto codecs = CodecCapability::availableCodecs(empty);
        QVERIFY(codecs.empty());
    }
};

QTEST_MAIN(TestCodecCapability)
#include "test_codec_capability.moc"
