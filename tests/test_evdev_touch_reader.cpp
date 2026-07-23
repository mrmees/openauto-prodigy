#include <QtTest>
#include <QElapsedTimer>
#include <QSignalSpy>

#include <oaa/HU/Handlers/InputChannelHandler.hpp>
#include "oaa/input/InputEventIndicationMessage.pb.h"

#include "core/aa/EvdevTouchReader.hpp"

#include <cerrno>
#include <poll.h>

namespace oap::aa {

class EvdevTouchReaderTestAccess {
public:
    static void configure(EvdevTouchReader& reader, int axisWidth = 1000,
                          int axisHeight = 1000)
    {
        reader.screenWidth_ = axisWidth;
        reader.screenHeight_ = axisHeight;
        reader.applyPendingMapping(true);
    }

    static void setSlot(EvdevTouchReader& reader, int slot, int trackingId,
                        int x, int y)
    {
        reader.slots_[slot].trackingId = trackingId;
        reader.slots_[slot].x = x;
        reader.slots_[slot].y = y;
        reader.slots_[slot].dirty = true;
    }

    static void process(EvdevTouchReader& reader) { reader.processSync(); }

    static void applyGrab(EvdevTouchReader& reader)
    {
        reader.fd_ = 123;
        reader.applyRequestedGrab();
        reader.fd_ = -1;
    }

    static void reset(EvdevTouchReader& reader) { reader.resetTouchState(); }

    static int aaWidth(const EvdevTouchReader& reader) { return reader.aaWidth_; }
    static int contentWidth(const EvdevTouchReader& reader) { return reader.contentWidth_; }
    static int navbarThickness(const EvdevTouchReader& reader)
    {
        return reader.navbarThickness_;
    }
};

} // namespace oap::aa

using oap::aa::EvdevTouchReader;
using oap::aa::EvdevTouchReaderTestAccess;

namespace {

oaa::proto::messages::InputEventIndication indicationAt(const QSignalSpy& spy, int index)
{
    const QByteArray payload = spy.at(index).at(2).toByteArray();
    oaa::proto::messages::InputEventIndication indication;
    if (!indication.ParseFromArray(payload.constData(), payload.size()))
        qFatal("Failed to parse InputEventIndication");
    return indication;
}

class DeviceLossReader final : public EvdevTouchReader {
public:
    enum class Failure { PollError, PollHangup, PollInvalid, ReadError, ShortRead };

    explicit DeviceLossReader(Failure failure)
        : EvdevTouchReader(nullptr, "/not-used", 1280, 720, 1024, 600)
        , failure_(failure)
    {
    }

    int openCalls = 0;
    int closeCalls = 0;
    int reconnectWaits = 0;

protected:
    bool openDevice() override
    {
        ++openCalls;
        return true;
    }
    void closeDevice() override { ++closeCalls; }
    void queryAxisRanges() override {}
    int pollDevice(short& revents, int) override
    {
        if (openCalls == 1) {
            if (failure_ == Failure::PollError) {
                errno = EIO;
                revents = 0;
                return -1;
            }
            if (failure_ == Failure::PollHangup)
                revents = POLLHUP;
            else if (failure_ == Failure::PollInvalid)
                revents = POLLNVAL;
            else
                revents = POLLIN;
            return 1;
        }
        requestStop();
        revents = 0;
        return 0;
    }
    ssize_t readDeviceEvent(input_event&) override
    {
        if (failure_ == Failure::ReadError) {
            errno = EIO;
            return -1;
        }
        return sizeof(input_event) - 1;
    }
    bool setDeviceGrab(bool) override { return true; }
    bool waitForReconnect() override
    {
        ++reconnectWaits;
        return true;
    }

private:
    Failure failure_;
};

class MissingDeviceReader final : public EvdevTouchReader {
public:
    MissingDeviceReader()
        : EvdevTouchReader(nullptr, "/not-used", 1280, 720, 1024, 600)
    {
    }

    std::atomic<int> openCalls{0};

protected:
    bool openDevice() override
    {
        ++openCalls;
        return false;
    }
};

class GrabRecordingReader final : public EvdevTouchReader {
public:
    GrabRecordingReader()
        : EvdevTouchReader(nullptr, "/not-used", 1280, 720, 1024, 600)
    {
    }

    QList<bool> applied;

protected:
    bool setDeviceGrab(bool grabbed) override
    {
        applied.push_back(grabbed);
        return true;
    }
};

} // namespace

class TestEvdevTouchReader : public QObject {
    Q_OBJECT

private slots:
    void sameReportDoubleDownAndUpUsesMotionEventOrdering();
    void sameReportReplacementReleasesBeforePressing();
    void claimedPointerNeverEntersPhoneArray();
    void videoMappingUpdateIsAtomicAndRetainsNavbarThickness();
    void grabMutationIsAppliedAtReaderBoundary();
    void ownershipLossCancelsPhoneVisiblePointers();
    void deviceLossReopens_data();
    void deviceLossReopens();
    void stopInterruptsReconnectWait();
};

void TestEvdevTouchReader::sameReportDoubleDownAndUpUsesMotionEventOrdering()
{
    oaa::hu::InputChannelHandler input;
    input.onChannelOpened();
    QSignalSpy sendSpy(&input, &oaa::IChannelHandler::sendRequested);
    oap::aa::TouchHandler touch;
    touch.setHandler(&input);
    EvdevTouchReader reader(&touch, "/not-used", 1000, 1000, 1000, 1000);
    EvdevTouchReaderTestAccess::configure(reader);

    EvdevTouchReaderTestAccess::setSlot(reader, 2, 20, 100, 200);
    EvdevTouchReaderTestAccess::setSlot(reader, 5, 50, 700, 800);
    EvdevTouchReaderTestAccess::process(reader);

    QCOMPARE(sendSpy.count(), 2);
    auto down = indicationAt(sendSpy, 0).touch_event();
    QCOMPARE(down.touch_action(), 0);
    QCOMPARE(down.action_index(), 0u);
    QCOMPARE(down.touch_location_size(), 1);
    QCOMPARE(down.touch_location(0).pointer_id(), 2u);

    auto pointerDown = indicationAt(sendSpy, 1).touch_event();
    QCOMPARE(pointerDown.touch_action(), 5);
    QCOMPARE(pointerDown.action_index(), 1u);
    QCOMPARE(pointerDown.touch_location_size(), 2);
    QCOMPARE(pointerDown.touch_location(0).pointer_id(), 2u);
    QCOMPARE(pointerDown.touch_location(1).pointer_id(), 5u);

    EvdevTouchReaderTestAccess::setSlot(reader, 2, -1, 100, 200);
    EvdevTouchReaderTestAccess::setSlot(reader, 5, -1, 700, 800);
    EvdevTouchReaderTestAccess::process(reader);

    QCOMPARE(sendSpy.count(), 4);
    auto pointerUp = indicationAt(sendSpy, 2).touch_event();
    QCOMPARE(pointerUp.touch_action(), 6);
    QCOMPARE(pointerUp.action_index(), 0u);
    QCOMPARE(pointerUp.touch_location_size(), 2);
    QCOMPARE(pointerUp.touch_location(0).pointer_id(), 2u);
    QCOMPARE(pointerUp.touch_location(1).pointer_id(), 5u);

    auto up = indicationAt(sendSpy, 3).touch_event();
    QCOMPARE(up.touch_action(), 1);
    QCOMPARE(up.action_index(), 0u);
    QCOMPARE(up.touch_location_size(), 1);
    QCOMPARE(up.touch_location(0).pointer_id(), 5u);
}

void TestEvdevTouchReader::sameReportReplacementReleasesBeforePressing()
{
    oaa::hu::InputChannelHandler input;
    input.onChannelOpened();
    QSignalSpy sendSpy(&input, &oaa::IChannelHandler::sendRequested);
    oap::aa::TouchHandler touch;
    touch.setHandler(&input);
    EvdevTouchReader reader(&touch, "/not-used", 1000, 1000, 1000, 1000);
    EvdevTouchReaderTestAccess::configure(reader);

    EvdevTouchReaderTestAccess::setSlot(reader, 2, 20, 100, 200);
    EvdevTouchReaderTestAccess::process(reader);
    QCOMPARE(sendSpy.count(), 1);

    EvdevTouchReaderTestAccess::setSlot(reader, 2, -1, 100, 200);
    EvdevTouchReaderTestAccess::setSlot(reader, 5, 50, 700, 800);
    EvdevTouchReaderTestAccess::process(reader);

    QCOMPARE(sendSpy.count(), 3);
    const auto up = indicationAt(sendSpy, 1).touch_event();
    QCOMPARE(up.touch_action(), 1);
    QCOMPARE(up.touch_location_size(), 1);
    QCOMPARE(up.touch_location(0).pointer_id(), 2u);

    const auto down = indicationAt(sendSpy, 2).touch_event();
    QCOMPARE(down.touch_action(), 0);
    QCOMPARE(down.touch_location_size(), 1);
    QCOMPARE(down.touch_location(0).pointer_id(), 5u);
}

void TestEvdevTouchReader::claimedPointerNeverEntersPhoneArray()
{
    oaa::hu::InputChannelHandler input;
    input.onChannelOpened();
    QSignalSpy sendSpy(&input, &oaa::IChannelHandler::sendRequested);
    oap::aa::TouchHandler touch;
    touch.setHandler(&input);
    EvdevTouchReader reader(&touch, "/not-used", 1000, 1000, 1000, 1000);
    EvdevTouchReaderTestAccess::configure(reader);

    int zoneEvents = 0;
    reader.router().setZones({{"navbar", 100, 0, 0, 250, 1000,
        [&zoneEvents](int, float, float, oap::aa::TouchEvent) { ++zoneEvents; }}});

    EvdevTouchReaderTestAccess::setSlot(reader, 0, 10, 100, 500);
    EvdevTouchReaderTestAccess::setSlot(reader, 1, 11, 750, 500);
    EvdevTouchReaderTestAccess::process(reader);

    QCOMPARE(zoneEvents, 1);
    QCOMPARE(sendSpy.count(), 1);
    const auto down = indicationAt(sendSpy, 0).touch_event();
    QCOMPARE(down.touch_action(), 0);
    QCOMPARE(down.touch_location_size(), 1);
    QCOMPARE(down.touch_location(0).pointer_id(), 1u);

    EvdevTouchReaderTestAccess::setSlot(reader, 0, 10, 150, 550);
    EvdevTouchReaderTestAccess::setSlot(reader, 1, 11, 800, 550);
    EvdevTouchReaderTestAccess::process(reader);
    QCOMPARE(zoneEvents, 2);
    QCOMPARE(sendSpy.count(), 2);
    const auto move = indicationAt(sendSpy, 1).touch_event();
    QCOMPARE(move.touch_action(), 2);
    QCOMPARE(move.touch_location_size(), 1);
    QCOMPARE(move.touch_location(0).pointer_id(), 1u);
}

void TestEvdevTouchReader::videoMappingUpdateIsAtomicAndRetainsNavbarThickness()
{
    EvdevTouchReader reader(nullptr, "/not-used", 1280, 720, 1024, 600);
    reader.setNavbar(true, 112, "right");
    reader.setVideoMapping(1920, 1080, 1704, 1080);
    EvdevTouchReaderTestAccess::configure(reader, 4095, 4095);

    QCOMPARE(EvdevTouchReaderTestAccess::aaWidth(reader), 1920);
    QCOMPARE(EvdevTouchReaderTestAccess::contentWidth(reader), 1704);
    QCOMPARE(EvdevTouchReaderTestAccess::navbarThickness(reader), 112);
}

void TestEvdevTouchReader::grabMutationIsAppliedAtReaderBoundary()
{
    GrabRecordingReader reader;
    reader.grab();
    QCOMPARE(reader.applied.size(), 0);
    EvdevTouchReaderTestAccess::applyGrab(reader);
    QCOMPARE(reader.applied, QList<bool>{true});

    reader.ungrab();
    QCOMPARE(reader.applied.size(), 1);
    EvdevTouchReaderTestAccess::applyGrab(reader);
    QCOMPARE(reader.applied, (QList<bool>{true, false}));
}

void TestEvdevTouchReader::ownershipLossCancelsPhoneVisiblePointers()
{
    oaa::hu::InputChannelHandler input;
    input.onChannelOpened();
    QSignalSpy sendSpy(&input, &oaa::IChannelHandler::sendRequested);
    oap::aa::TouchHandler touch;
    touch.setHandler(&input);
    EvdevTouchReader reader(&touch, "/not-used", 1000, 1000, 1000, 1000);
    EvdevTouchReaderTestAccess::configure(reader);

    EvdevTouchReaderTestAccess::setSlot(reader, 2, 20, 100, 200);
    EvdevTouchReaderTestAccess::setSlot(reader, 5, 50, 700, 800);
    EvdevTouchReaderTestAccess::process(reader);
    QCOMPARE(sendSpy.count(), 2);

    // Both ungrab and device-loss paths converge on resetTouchState().
    EvdevTouchReaderTestAccess::reset(reader);
    QCOMPARE(sendSpy.count(), 4);
    const auto pointerUp = indicationAt(sendSpy, 2).touch_event();
    QCOMPARE(pointerUp.touch_action(), 6);
    QCOMPARE(pointerUp.touch_location_size(), 2);
    QCOMPARE(pointerUp.touch_location(0).pointer_id(), 2u);
    QCOMPARE(pointerUp.touch_location(1).pointer_id(), 5u);
    const auto up = indicationAt(sendSpy, 3).touch_event();
    QCOMPARE(up.touch_action(), 1);
    QCOMPARE(up.touch_location_size(), 1);
    QCOMPARE(up.touch_location(0).pointer_id(), 5u);

    EvdevTouchReaderTestAccess::reset(reader);
    QCOMPARE(sendSpy.count(), 4);
}

void TestEvdevTouchReader::deviceLossReopens_data()
{
    QTest::addColumn<int>("failure");
    QTest::newRow("poll-error") << static_cast<int>(DeviceLossReader::Failure::PollError);
    QTest::newRow("poll-hangup") << static_cast<int>(DeviceLossReader::Failure::PollHangup);
    QTest::newRow("poll-invalid") << static_cast<int>(DeviceLossReader::Failure::PollInvalid);
    QTest::newRow("read-error") << static_cast<int>(DeviceLossReader::Failure::ReadError);
    QTest::newRow("short-read") << static_cast<int>(DeviceLossReader::Failure::ShortRead);
}

void TestEvdevTouchReader::deviceLossReopens()
{
    QFETCH(int, failure);
    DeviceLossReader reader(static_cast<DeviceLossReader::Failure>(failure));
    reader.start();
    QVERIFY(reader.wait(1000));
    QCOMPARE(reader.openCalls, 2);
    QCOMPARE(reader.reconnectWaits, 1);
    QVERIFY(reader.closeCalls >= 2);
}

void TestEvdevTouchReader::stopInterruptsReconnectWait()
{
    MissingDeviceReader reader;
    reader.start();
    QTRY_VERIFY_WITH_TIMEOUT(reader.openCalls.load() > 0, 500);

    QElapsedTimer elapsed;
    elapsed.start();
    reader.requestStop();
    QVERIFY(reader.wait(500));
    QVERIFY2(elapsed.elapsed() < 500, "requestStop did not wake reconnect wait");
}

QTEST_GUILESS_MAIN(TestEvdevTouchReader)
#include "test_evdev_touch_reader.moc"
