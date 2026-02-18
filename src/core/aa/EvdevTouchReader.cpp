#include "EvdevTouchReader.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <cstring>

namespace oap {
namespace aa {

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

    BOOST_LOG_TRIVIAL(info) << "[EvdevTouch] Opened " << devicePath_
                            << " (range: " << screenWidth_ << "x" << screenHeight_
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
            int ax = static_cast<int>(static_cast<float>(slots_[i].x) / screenWidth_ * aaWidth_);
            int ay = static_cast<int>(static_cast<float>(slots_[i].y) / screenHeight_ * aaHeight_);
            pointers.push_back({ax, ay, i});  // pointer_id = slot index (stable per finger)
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
                                    << " active=" << nowActive;
        }
        else if (wasActive && !isActive) {
            // Finger lifted — need to include this pointer in the array at its last position
            // Rebuild pointers INCLUDING the lifted finger
            std::vector<TouchHandler::Pointer> withLifted;
            for (int j = 0; j < MAX_SLOTS; ++j) {
                if (slots_[j].trackingId >= 0 || (j == i && prevSlots_[j].trackingId >= 0)) {
                    int sx = (j == i) ? prevSlots_[j].x : slots_[j].x;
                    int sy = (j == i) ? prevSlots_[j].y : slots_[j].y;
                    int ax = static_cast<int>(static_cast<float>(sx) / screenWidth_ * aaWidth_);
                    int ay = static_cast<int>(static_cast<float>(sy) / screenHeight_ * aaHeight_);
                    withLifted.push_back({ax, ay, j});
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
