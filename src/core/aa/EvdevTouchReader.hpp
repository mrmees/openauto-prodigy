#pragma once

#include <QThread>
#include <array>
#include <vector>
#include <string>
#include <linux/input.h>
#include <boost/log/trivial.hpp>
#include "TouchHandler.hpp"

namespace oap {
namespace aa {

// Reads multi-touch events directly from a Linux evdev device (MT Type B protocol).
// Bypasses Qt's touch input entirely to avoid Wayland/evdev plugin conflicts.
// Tracks full slot state and sends properly formed AA touch messages.
//
// Supports letterboxed video: if the AA video aspect ratio doesn't match the
// physical display, touches in the black bar area are clamped to the video edge.
class EvdevTouchReader : public QThread {
    Q_OBJECT

public:
    static constexpr int MAX_SLOTS = 10;

    struct Slot {
        int trackingId = -1;  // -1 = inactive
        int x = 0;
        int y = 0;
        bool dirty = false;   // changed since last SYN
    };

    explicit EvdevTouchReader(TouchHandler* handler,
                              const std::string& devicePath,
                              int screenWidth, int screenHeight,
                              int aaWidth, int aaHeight,
                              int displayWidth, int displayHeight,
                              QObject* parent = nullptr)
        : QThread(parent)
        , handler_(handler)
        , devicePath_(devicePath)
        , screenWidth_(screenWidth)
        , screenHeight_(screenHeight)
        , aaWidth_(aaWidth)
        , aaHeight_(aaHeight)
        , displayWidth_(displayWidth)
        , displayHeight_(displayHeight)
    {
        slots_.fill(Slot{});
        computeLetterbox();
    }

    void requestStop() { stopRequested_ = true; }

protected:
    void run() override;

private:
    void processSync();
    int countActive() const;
    int slotToArrayIndex(int slot) const;
    void computeLetterbox();

    // Map raw evdev coordinate to AA coordinate, accounting for letterbox
    int mapX(int rawX) const;
    int mapY(int rawY) const;

    TouchHandler* handler_;
    std::string devicePath_;
    int screenWidth_;   // evdev axis max (e.g. 4095)
    int screenHeight_;  // evdev axis max (e.g. 4095)
    int aaWidth_;       // AA touch coordinate space (video resolution)
    int aaHeight_;
    int displayWidth_;  // physical display pixels
    int displayHeight_;

    // Letterbox: video area within evdev coordinate space
    float videoEvdevX0_ = 0;  // left edge of video in evdev coords
    float videoEvdevY0_ = 0;  // top edge of video in evdev coords
    float videoEvdevW_ = 0;   // width of video area in evdev coords
    float videoEvdevH_ = 0;   // height of video area in evdev coords

    std::array<Slot, MAX_SLOTS> slots_;
    std::array<Slot, MAX_SLOTS> prevSlots_;  // state before this SYN
    int currentSlot_ = 0;
    bool stopRequested_ = false;
    int fd_ = -1;
};

} // namespace aa
} // namespace oap
