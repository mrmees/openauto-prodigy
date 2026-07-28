#include <QTest>

#include <oaa/Channel/MessageIds.hpp>

#include "oaa/av/AVChannelData.pb.h"
#include "oaa/av/AndroidKeycodeEnum.pb.h"
#include "oaa/av/MediaCodecTypeEnum.pb.h"
#include "oaa/input/InputChannelConfigData.pb.h"

class TestProtocolConstants : public QObject {
    Q_OBJECT

private slots:
    void avCodecValuesRemainWireCompatible()
    {
        using Codec = oaa::proto::enums::MediaCodecType;
        QCOMPARE(static_cast<int>(Codec::MEDIA_CODEC_AUDIO_PCM), 1);
        QCOMPARE(static_cast<int>(Codec::MEDIA_CODEC_VIDEO_H264_BP), 3);

        oaa::proto::data::AVChannel audio;
        audio.set_stream_type(Codec::MEDIA_CODEC_AUDIO_PCM);
        QCOMPARE(QByteArray::fromStdString(audio.SerializeAsString()),
                 QByteArray::fromHex("0801"));

        oaa::proto::data::AVChannel video;
        video.set_stream_type(Codec::MEDIA_CODEC_VIDEO_H264_BP);
        QCOMPARE(QByteArray::fromStdString(video.SerializeAsString()),
                 QByteArray::fromHex("0803"));
    }

    void logicalDisplayFieldsRoundTripAtAuditedTags()
    {
        oaa::proto::data::AVChannel video;
        video.set_display_id(1);
        QCOMPARE(QByteArray::fromStdString(video.SerializeAsString()),
                 QByteArray::fromHex("3001"));

        oaa::proto::data::InputChannelConfig input;
        input.set_display_id(1);
        QCOMPARE(QByteArray::fromStdString(input.SerializeAsString()),
                 QByteArray::fromHex("2801"));
    }

    void auxiliarySelectorKeycodesMatchAuditedValues()
    {
        using Keycode = oaa::proto::enums::AndroidKeycode;
        QCOMPARE(static_cast<int>(Keycode::KEYCODE_NAVIGATION), 65538);
        QCOMPARE(static_cast<int>(Keycode::KEYCODE_TURN_CARD), 65544);
    }

    void manualAvMessageIdsMatchAudited17_3Map()
    {
        QCOMPARE(oaa::AVMessageId::AV_MEDIA_WITH_TIMESTAMP, uint16_t{0x0000});
        QCOMPARE(oaa::AVMessageId::AV_MEDIA_INDICATION, uint16_t{0x0001});
        QCOMPARE(oaa::AVMessageId::SETUP_REQUEST, uint16_t{0x8000});
        QCOMPARE(oaa::AVMessageId::START_INDICATION, uint16_t{0x8001});
        QCOMPARE(oaa::AVMessageId::STOP_INDICATION, uint16_t{0x8002});
        QCOMPARE(oaa::AVMessageId::SETUP_RESPONSE, uint16_t{0x8003});
        QCOMPARE(oaa::AVMessageId::ACK_INDICATION, uint16_t{0x8004});
        QCOMPARE(oaa::AVMessageId::INPUT_OPEN_REQUEST, uint16_t{0x8005});
        QCOMPARE(oaa::AVMessageId::INPUT_OPEN_RESPONSE, uint16_t{0x8006});
        QCOMPARE(oaa::AVMessageId::VIDEO_FOCUS_REQUEST, uint16_t{0x8007});
        QCOMPARE(oaa::AVMessageId::VIDEO_FOCUS_INDICATION, uint16_t{0x8008});
        QCOMPARE(oaa::AVMessageId::UPDATE_UI_CONFIG_TO_PHONE, uint16_t{0x8009});
        QCOMPARE(oaa::AVMessageId::UPDATE_UI_CONFIG_FROM_PHONE, uint16_t{0x800A});
        QCOMPARE(oaa::AVMessageId::AUDIO_UNDERFLOW, uint16_t{0x800B});
        QCOMPARE(oaa::AVMessageId::ACTION_TAKEN, uint16_t{0x800C});
        QCOMPARE(oaa::AVMessageId::OVERLAY_PARAMETERS, uint16_t{0x800D});
        QCOMPARE(oaa::AVMessageId::OVERLAY_START, uint16_t{0x800E});
        QCOMPARE(oaa::AVMessageId::OVERLAY_STOP, uint16_t{0x800F});
        QCOMPARE(oaa::AVMessageId::RESERVED_8010, uint16_t{0x8010});
        QCOMPARE(oaa::AVMessageId::UI_CONFIG_REQUEST, uint16_t{0x8011});
        QCOMPARE(oaa::AVMessageId::UPDATE_HU_UI_CONFIG_RESPONSE, uint16_t{0x8012});
        QCOMPARE(oaa::AVMessageId::MEDIA_STATS, uint16_t{0x8013});
        QCOMPARE(oaa::AVMessageId::MEDIA_OPTIONS, uint16_t{0x8014});
        QCOMPARE(oaa::AVMessageId::CRITICAL_UI_NOTIFICATION, uint16_t{0x8015});
    }

    void navigationMessageIdsMatchAudited17_3Map()
    {
        QCOMPARE(oaa::NavigationMessageId::VEHICLE_ENERGY_FORECAST,
                 uint16_t{0x8008});
    }
};

QTEST_MAIN(TestProtocolConstants)
#include "test_protocol_constants.moc"
