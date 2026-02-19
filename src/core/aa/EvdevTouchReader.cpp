#include "EvdevTouchReader.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <cstring>
#include <algorithm>

namespace oap {
namespace aa {

void EvdevTouchReader::computeLetterbox()
{
    // Compute where the video (aaWidth x aaHeight) fits within the physical
    // display (displayWidth x displayHeight), PreserveAspectFit style.
    // Then map that region into evdev coordinate space (0..screenWidth, 0..screenHeight).

    float displayAspect = static_cast<float>(displayWidth_) / displayHeight_;
    float videoAspect = static_cast<float>(aaWidth_) / aaHeight_;

    float videoPixelW, videoPixelH, videoPixelX0, videoPixelY0;

    if (videoAspect > displayAspect) {
        // Video is wider than display — pillarbox (bars top/bottom... wait, wider = fit to width)
        videoPixelW = displayWidth_;
        videoPixelH = displayWidth_ / videoAspect;
        videoPixelX0 = 0;
        videoPixelY0 = (displayHeight_ - videoPixelH) / 2.0f;
    } else {
        // Video is taller than display — fit to height, bars on sides
        videoPixelH = displayHeight_;
        videoPixelW = displayHeight_ * videoAspect;
        videoPixelX0 = (displayWidth_ - videoPixelW) / 2.0f;
        videoPixelY0 = 0;
    }

    // Convert pixel positions to evdev coordinate space
    // evdev 0..screenWidth maps to display 0..displayWidth
    float evdevPerPixelX = static_cast<float>(screenWidth_) / displayWidth_;
    float evdevPerPixelY = static_cast<float>(screenHeight_) / displayHeight_;

    videoEvdevX0_ = videoPixelX0 * evdevPerPixelX;
    videoEvdevY0_ = videoPixelY0 * evdevPerPixelY;
    videoEvdevW_ = videoPixelW * evdevPerPixelX;
    videoEvdevH_ = videoPixelH * evdevPerPixelY;

    BOOST_LOG_TRIVIAL(info) << "[EvdevTouch] Letterbox: video " << aaWidth_ << "x" << aaHeight_
                            << " in display " << displayWidth_ << "x" << displayHeight_
                            << " -> pixel area " << videoPixelW << "x" << videoPixelH
                            << " at (" << videoPixelX0 << "," << videoPixelY0 << ")"
                            << " | evdev area (" << videoEvdevX0_ << "," << videoEvdevY0_
                            << ") " << videoEvdevW_ << "x" << videoEvdevH_;
}

int EvdevTouchReader::mapX(int rawX) const
{
    float rel = (rawX - videoEvdevX0_) / videoEvdevW_;
    rel = std::clamp(rel, 0.0f, 1.0f);
    return static_cast<int>(rel * aaWidth_);
}

int EvdevTouchReader::mapY(int rawY) const
{
    float rel = (rawY - videoEvdevY0_) / videoEvdevH_;
    rel = std::clamp(rel, 0.0f, 1.0f);
    return static_cast<int>(rel * aaHeight_);
}

void EvdevTouchReader::run()
{
    fd_ = ::open(devicePath_.c_str(), O_RDONLY);
    if (fd_ < 0) {
        BOOST_LOG_TRIVIAL(error) << "[EvdevTouch] Failed to open " << devicePath_
                                 << ": " << strerror(errno);
        return;
    }

    // Exclusive grab — prevents libinput/Wayland from seeing these events
    if (::ioctl(fd_, EVIOCGRAB, 1) < 0) {
        BOOST_LOG_TRIVIAL(warning) << "[EvdevTouch] EVIOCGRAB failed: " << strerror(errno)
                                   << " (multi-touch may duplicate)";
    }

    // Read axis ranges for coordinate scaling
    struct input_absinfo absX{}, absY{};
    if (::ioctl(fd_, EVIOCGABS(ABS_MT_POSITION_X), &absX) == 0)
        screenWidth_ = absX.maximum;
    if (::ioctl(fd_, EVIOCGABS(ABS_MT_POSITION_Y), &absY) == 0)
        screenHeight_ = absY.maximum;

    // Recompute letterbox with actual axis ranges
    computeLetterbox();

    BOOST_LOG_TRIVIAL(info) << "[EvdevTouch] Opened " << devicePath_
                            << " (evdev: " << screenWidth_ << "x" << screenHeight_
                            << " -> AA " << aaWidth_ << "x" << aaHeight_ << ")";

    prevSlots_ = slots_;

    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLIN;

    while (!stopRequested_) {
        int ret = ::poll(&pfd, 1, 100);  // 100ms timeout for stop check
        if (ret <= 0) continue;

        struct input_event ev;
        ssize_t n = ::read(fd_, &ev, sizeof(ev));
        if (n != sizeof(ev)) continue;

        switch (ev.type) {
        case EV_ABS:
            switch (ev.code) {
            case ABS_MT_SLOT:
                currentSlot_ = ev.value;
                if (currentSlot_ >= MAX_SLOTS) currentSlot_ = MAX_SLOTS - 1;
                break;
            case ABS_MT_TRACKING_ID:
                slots_[currentSlot_].trackingId = ev.value;
                slots_[currentSlot_].dirty = true;
                break;
            case ABS_MT_POSITION_X:
                slots_[currentSlot_].x = ev.value;
                slots_[currentSlot_].dirty = true;
                break;
            case ABS_MT_POSITION_Y:
                slots_[currentSlot_].y = ev.value;
                slots_[currentSlot_].dirty = true;
                break;
            }
            break;

        case EV_SYN:
            if (ev.code == SYN_REPORT)
                processSync();
            break;
        }
    }

    ::ioctl(fd_, EVIOCGRAB, 0);
    ::close(fd_);
    fd_ = -1;
    BOOST_LOG_TRIVIAL(info) << "[EvdevTouch] Reader thread stopped";
}

int EvdevTouchReader::countActive() const
{
    int n = 0;
    for (auto& s : slots_)
        if (s.trackingId >= 0) ++n;
    return n;
}

// Map a slot index to its position in the active-pointers array
int EvdevTouchReader::slotToArrayIndex(int slot) const
{
    int idx = 0;
    for (int i = 0; i < slot; ++i)
        if (slots_[i].trackingId >= 0) ++idx;
    return idx;
}

void EvdevTouchReader::processSync()
{
    // Determine what changed: new fingers, lifted fingers, moved fingers
    int prevActive = 0, nowActive = 0;
    for (int i = 0; i < MAX_SLOTS; ++i) {
        if (prevSlots_[i].trackingId >= 0) ++prevActive;
        if (slots_[i].trackingId >= 0) ++nowActive;
    }

    // Build the full pointer array (all currently active slots)
    using Pointer = TouchHandler::Pointer;
    std::vector<Pointer> pointers;
    for (int i = 0; i < MAX_SLOTS; ++i) {
        if (slots_[i].trackingId >= 0) {
            pointers.push_back({mapX(slots_[i].x), mapY(slots_[i].y), i});
        }
    }

    // Check for finger down/up events first
    for (int i = 0; i < MAX_SLOTS; ++i) {
        if (!slots_[i].dirty) continue;

        bool wasActive = prevSlots_[i].trackingId >= 0;
        bool isActive = slots_[i].trackingId >= 0;

        if (!wasActive && isActive) {
            // Finger pressed
            int action = (prevActive == 0) ? 0 : 5;  // DOWN or POINTER_DOWN
            int actionIdx = slotToArrayIndex(i);

            handler_->sendTouchIndication(pointers.size(), pointers.data(),
                                          actionIdx, action);
            prevActive = nowActive;  // update for subsequent events in same SYN

            BOOST_LOG_TRIVIAL(info) << "[EvdevTouch] DOWN slot=" << i
                                    << " actionIdx=" << actionIdx
                                    << " active=" << nowActive
                                    << " raw=(" << slots_[i].x << "," << slots_[i].y << ")"
                                    << " aa=(" << pointers[actionIdx].x << "," << pointers[actionIdx].y << ")";
        }
        else if (wasActive && !isActive) {
            // Finger lifted — need to include this pointer in the array at its last position
            std::vector<Pointer> withLifted;
            for (int j = 0; j < MAX_SLOTS; ++j) {
                if (slots_[j].trackingId >= 0 || (j == i && prevSlots_[j].trackingId >= 0)) {
                    int sx = (j == i) ? prevSlots_[j].x : slots_[j].x;
                    int sy = (j == i) ? prevSlots_[j].y : slots_[j].y;
                    withLifted.push_back({mapX(sx), mapY(sy), j});
                }
            }

            // Find this finger's index in the withLifted array
            int actionIdx = 0;
            for (int j = 0; j < (int)withLifted.size(); ++j) {
                if (withLifted[j].id == i) { actionIdx = j; break; }
            }

            int action = (nowActive == 0) ? 1 : 6;  // UP or POINTER_UP
            handler_->sendTouchIndication(withLifted.size(), withLifted.data(),
                                          actionIdx, action);

            BOOST_LOG_TRIVIAL(info) << "[EvdevTouch] UP slot=" << i
                                    << " actionIdx=" << actionIdx
                                    << " active=" << nowActive;
        }
    }

    // Send MOVE if any active slots changed position (and no down/up happened)
    bool anyMoved = false;
    for (int i = 0; i < MAX_SLOTS; ++i) {
        if (slots_[i].dirty && slots_[i].trackingId >= 0 && prevSlots_[i].trackingId >= 0) {
            if (slots_[i].x != prevSlots_[i].x || slots_[i].y != prevSlots_[i].y) {
                anyMoved = true;
                break;
            }
        }
    }

    if (anyMoved && !pointers.empty()) {
        handler_->sendTouchIndication(pointers.size(), pointers.data(), 0, 2);  // MOVE
    }

    // Clear dirty flags and save state
    for (int i = 0; i < MAX_SLOTS; ++i)
        slots_[i].dirty = false;
    prevSlots_ = slots_;
}

} // namespace aa
} // namespace oap
