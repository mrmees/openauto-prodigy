#include <QTest>
#include <QByteArray>

// Replicate the isKeyframe logic for unit testing (the real one is a static
// function in VideoDecoder.cpp, not directly accessible from tests).
static bool isKeyframe(const QByteArray& data, bool codecIsH265)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data.constData());
    int len = data.size();
    for (int i = 0; i + 3 <= len; ++i) {
        if (p[i] == 0 && p[i+1] == 0) {
            int nalStart = -1;
            if (p[i+2] == 1) {
                nalStart = i + 3;
            } else if (p[i+2] == 0 && i + 3 < len && p[i+3] == 1) {
                nalStart = i + 4;
            }
            if (nalStart >= 0 && nalStart < len) {
                uint8_t firstByte = p[nalStart];
                if (codecIsH265) {
                    uint8_t h265Type = (firstByte >> 1) & 0x3F;
                    if (h265Type == 19 || h265Type == 20 || h265Type == 32 || h265Type == 33 || h265Type == 34)
                        return true;
                } else {
                    uint8_t h264Type = firstByte & 0x1F;
                    if (h264Type == 5 || h264Type == 7 || h264Type == 8)
                        return true;
                }
            }
        }
    }
    return false;
}

class TestVideoDecodeQueue : public QObject {
    Q_OBJECT

private slots:
    // --- H.264 keyframe detection ---

    void testH264IdrWith4ByteStartCode()
    {
        // 00 00 00 01 65 — NAL type 5 (IDR slice)
        QByteArray idr;
        idr.append('\x00'); idr.append('\x00'); idr.append('\x00'); idr.append('\x01');
        idr.append('\x65'); // 0b01100101 → type = 5
        QVERIFY(isKeyframe(idr, false));
    }

    void testH264SpsWith4ByteStartCode()
    {
        // 00 00 00 01 67 — NAL type 7 (SPS)
        QByteArray sps;
        sps.append('\x00'); sps.append('\x00'); sps.append('\x00'); sps.append('\x01');
        sps.append('\x67'); // 0b01100111 → type = 7
        QVERIFY(isKeyframe(sps, false));
    }

    void testH264PpsWith4ByteStartCode()
    {
        // 00 00 00 01 68 — NAL type 8 (PPS)
        QByteArray pps;
        pps.append('\x00'); pps.append('\x00'); pps.append('\x00'); pps.append('\x01');
        pps.append('\x68'); // 0b01101000 → type = 8
        QVERIFY(isKeyframe(pps, false));
    }

    void testH264IdrWith3ByteStartCode()
    {
        // 00 00 01 65 — 3-byte start code, NAL type 5 (IDR)
        QByteArray idr;
        idr.append('\x00'); idr.append('\x00'); idr.append('\x01');
        idr.append('\x65');
        QVERIFY(isKeyframe(idr, false));
    }

    void testH264NonIdrSlice()
    {
        // 00 00 00 01 41 — NAL type 1 (non-IDR coded slice, nal_ref_idc=2)
        QByteArray slice;
        slice.append('\x00'); slice.append('\x00'); slice.append('\x00'); slice.append('\x01');
        slice.append('\x41'); // 0b01000001 → type = 1
        QVERIFY(!isKeyframe(slice, false));
    }

    void testH264NonIdrSliceType2()
    {
        // 00 00 00 01 01 — NAL type 1 (P-frame, nal_ref_idc=0)
        QByteArray slice;
        slice.append('\x00'); slice.append('\x00'); slice.append('\x00'); slice.append('\x01');
        slice.append('\x01'); // 0b00000001 → type = 1
        QVERIFY(!isKeyframe(slice, false));
    }

    // --- H.265 keyframe detection ---

    void testH265IdrWRadl()
    {
        // 00 00 00 01 26 01 — NAL type 19 (IDR_W_RADL)
        // firstByte=0x26: (0x26 >> 1) & 0x3F = 0x13 = 19
        QByteArray idr;
        idr.append('\x00'); idr.append('\x00'); idr.append('\x00'); idr.append('\x01');
        idr.append('\x26'); // type 19
        idr.append('\x01'); // temporal_id
        QVERIFY(isKeyframe(idr, true));
    }

    void testH265IdrNLp()
    {
        // NAL type 20 (IDR_N_LP): (20 << 1) = 40 = 0x28
        QByteArray idr;
        idr.append('\x00'); idr.append('\x00'); idr.append('\x00'); idr.append('\x01');
        idr.append('\x28'); // type 20
        idr.append('\x01');
        QVERIFY(isKeyframe(idr, true));
    }

    void testH265Vps()
    {
        // NAL type 32 (VPS): (32 << 1) = 64 = 0x40
        QByteArray vps;
        vps.append('\x00'); vps.append('\x00'); vps.append('\x00'); vps.append('\x01');
        vps.append('\x40'); // type 32
        vps.append('\x01');
        QVERIFY(isKeyframe(vps, true));
    }

    void testH265Sps()
    {
        // NAL type 33 (SPS): (33 << 1) = 66 = 0x42
        QByteArray sps;
        sps.append('\x00'); sps.append('\x00'); sps.append('\x00'); sps.append('\x01');
        sps.append('\x42'); // type 33
        sps.append('\x01');
        QVERIFY(isKeyframe(sps, true));
    }

    void testH265Pps()
    {
        // NAL type 34 (PPS): (34 << 1) = 68 = 0x44
        QByteArray pps;
        pps.append('\x00'); pps.append('\x00'); pps.append('\x00'); pps.append('\x01');
        pps.append('\x44'); // type 34
        pps.append('\x01');
        QVERIFY(isKeyframe(pps, true));
    }

    void testH265NonIdrTrailR()
    {
        // NAL type 1 (TRAIL_R — non-IDR): (1 << 1) = 2 = 0x02
        QByteArray trail;
        trail.append('\x00'); trail.append('\x00'); trail.append('\x00'); trail.append('\x01');
        trail.append('\x02'); // type 1
        trail.append('\x01');
        QVERIFY(!isKeyframe(trail, true));
    }

    // --- Cross-codec disambiguation ---

    void testH264ByteNotMisdetectedAsH265()
    {
        // 0x41 is H.264 type 1 (non-IDR) but H.265 type 32 (VPS).
        // With correct codec hint, H.264 mode should NOT detect it as keyframe.
        QByteArray data;
        data.append('\x00'); data.append('\x00'); data.append('\x00'); data.append('\x01');
        data.append('\x41');
        QVERIFY(!isKeyframe(data, false));  // H.264 mode: type 1, not keyframe
        QVERIFY(isKeyframe(data, true));    // H.265 mode: type 32 (VPS), keyframe
    }

    // --- Edge cases ---

    void testEmptyData()
    {
        QVERIFY(!isKeyframe(QByteArray(), false));
        QVERIFY(!isKeyframe(QByteArray(), true));
    }

    void testTooShortData()
    {
        // Only a start code, no NAL byte
        QByteArray short1;
        short1.append('\x00'); short1.append('\x00'); short1.append('\x00'); short1.append('\x01');
        QVERIFY(!isKeyframe(short1, false));
    }

    void testSpsFollowedByIdr()
    {
        // Typical real H.264 stream: SPS + PPS + IDR in one buffer
        QByteArray multi;
        // SPS (type 7)
        multi.append('\x00'); multi.append('\x00'); multi.append('\x00'); multi.append('\x01');
        multi.append('\x67');
        multi.append('\x42'); // fake SPS payload
        multi.append('\x00'); multi.append('\x0a');
        // PPS (type 8)
        multi.append('\x00'); multi.append('\x00'); multi.append('\x00'); multi.append('\x01');
        multi.append('\x68');
        multi.append('\xce'); // fake PPS payload
        // IDR (type 5)
        multi.append('\x00'); multi.append('\x00'); multi.append('\x00'); multi.append('\x01');
        multi.append('\x65');
        QVERIFY(isKeyframe(multi, false));
    }

    void testNonKeyframeMultiNal()
    {
        // Multiple non-keyframe H.264 NALs — should return false
        QByteArray multi;
        // Non-IDR slice (type 1)
        multi.append('\x00'); multi.append('\x00'); multi.append('\x00'); multi.append('\x01');
        multi.append('\x41');
        multi.append('\x9a'); // payload
        // Another non-IDR slice (type 1)
        multi.append('\x00'); multi.append('\x00'); multi.append('\x00'); multi.append('\x01');
        multi.append('\x01');
        QVERIFY(!isKeyframe(multi, false));
    }

    void testGarbageData()
    {
        // Random bytes with no valid start code
        QByteArray garbage("hello world this is not a video frame");
        QVERIFY(!isKeyframe(garbage, false));
    }
};

QTEST_MAIN(TestVideoDecodeQueue)
#include "test_video_decode_queue.moc"
