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
    int effectiveDisplayW = displayWidth_;
    int effectiveDisplayX0 = 0;
    if (sidebarEnabled_ && sidebarPixelWidth_ > 0) {
        effectiveDisplayW = displayWidth_ - sidebarPixelWidth_;
        if (sidebarPosition_ == "left")
            effectiveDisplayX0 = sidebarPixelWidth_;
    }

    float videoAspect = static_cast<float>(aaWidth_) / aaHeight_;
    float displayAspect = static_cast<float>(effectiveDisplayW) / displayHeight_;

    float videoPixelW, videoPixelH, videoPixelX0, videoPixelY0;

    if (videoAspect > displayAspect) {
        videoPixelW = effectiveDisplayW;
        videoPixelH = effectiveDisplayW / videoAspect;
        videoPixelX0 = effectiveDisplayX0;
        videoPixelY0 = (displayHeight_ - videoPixelH) / 2.0f;
    } else {
        videoPixelH = displayHeight_;
        videoPixelW = displayHeight_ * videoAspect;
        videoPixelX0 = effectiveDisplayX0 + (effectiveDisplayW - videoPixelW) / 2.0f;
        videoPixelY0 = 0;
    }

    float evdevPerPixelX = static_cast<float>(screenWidth_) / displayWidth_;
    float evdevPerPixelY = static_cast<float>(screenHeight_) / displayHeight_;

    videoEvdevX0_ = videoPixelX0 * evdevPerPixelX;
    videoEvdevY0_ = videoPixelY0 * evdevPerPixelY;
    videoEvdevW_ = videoPixelW * evdevPerPixelX;
    videoEvdevH_ = videoPixelH * evdevPerPixelY;

    BOOST_LOG_TRIVIAL(info) << "[EvdevTouch] Letterbox: video " << aaWidth_ << "x" << aaHeight_
                            << " in display " << effectiveDisplayW << "x" << displayHeight_
                            << " at pixel (" << videoPixelX0 << "," << videoPixelY0 << ")"
                            << " | evdev (" << videoEvdevX0_ << "," << videoEvdevY0_
                            << ") " << videoEvdevW_ << "x" << videoEvdevH_;
}

void EvdevTouchReader::setSidebar(bool enabled, int width, const std::string& position)
{
    sidebarEnabled_ = enabled;
    sidebarPixelWidth_ = width;
    sidebarPosition_ = position;

    if (!enabled || width <= 0) return;

    float evdevPerPixelX = static_cast<float>(screenWidth_) / displayWidth_;
    float evdevPerPixelY = static_cast<float>(screenHeight_) / displayHeight_;

    if (position == "right") {
        int sidebarStartPx = displayWidth_ - width;
        sidebarEvdevX0_ = sidebarStartPx * evdevPerPixelX;
        sidebarEvdevX1_ = screenWidth_;
    } else {
        sidebarEvdevX0_ = 0;
        sidebarEvdevX1_ = width * evdevPerPixelX;
    }

    // Hit zones: top 40% = vol up, next 20% = vol down, bottom 25% = home
    sidebarVolUpY0_ = 0;
    sidebarVolUpY1_ = displayHeight_ * 0.40f * evdevPerPixelY;
    sidebarVolDownY0_ = sidebarVolUpY1_;
    sidebarVolDownY1_ = displayHeight_ * 0.60f * evdevPerPixelY;
    sidebarHomeY0_ = displayHeight_ * 0.75f * evdevPerPixelY;
    sidebarHomeY1_ = screenHeight_;

    BOOST_LOG_TRIVIAL(info) << "[EvdevTouch] Sidebar: " << position << " " << width << "px"
                            << ", evdev X: " << sidebarEvdevX0_ << "-" << sidebarEvdevX1_;
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

    // Don't grab on startup — let Wayland/libinput handle touch for the launcher UI.
    // The grab will be activated when AA connects via grab().

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

        // When not grabbed, discard events — Wayland/libinput is handling touch
        if (!grabbed_.load(std::memory_order_relaxed)) continue;

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

    if (grabbed_.load())
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

bool EvdevTouchReader::checkGesture()
{
    int nowActive = countActive();

    // Track max fingers in the current gesture window
    if (nowActive > 0 && prevActiveCount_ == 0) {
        // First finger down — start gesture window
        firstFingerTime_ = std::chrono::steady_clock::now();
        gestureMaxFingers_ = 1;
        gestureActive_ = false;
    } else if (nowActive > gestureMaxFingers_) {
        gestureMaxFingers_ = nowActive;
    }

    prevActiveCount_ = nowActive;

    // Check if we've reached the finger threshold within the time window
    if (gestureMaxFingers_ >= GESTURE_FINGER_COUNT && !gestureActive_) {
        auto elapsed = std::chrono::steady_clock::now() - firstFingerTime_;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        if (ms <= GESTURE_WINDOW_MS) {
            gestureActive_ = true;
            BOOST_LOG_TRIVIAL(info) << "[EvdevTouch] 3-finger gesture detected (" << ms << "ms)";
            emit gestureDetected();
        }
    }

    // When all fingers lift, end gesture suppression
    if (nowActive == 0 && gestureActive_) {
        gestureActive_ = false;
        gestureMaxFingers_ = 0;
    }

    return gestureActive_;
}

void EvdevTouchReader::processSync()
{
    // Check for 3-finger gesture — suppress touches if active
    if (checkGesture()) {
        // Clear dirty flags and save state, but don't forward to AA
        for (int i = 0; i < MAX_SLOTS; ++i)
            slots_[i].dirty = false;
        prevSlots_ = slots_;
        return;
    }

    // Check for sidebar touches — detect hits and suppress AA forwarding
    if (sidebarEnabled_) {
        bool anySidebarTouch = false;
        for (int i = 0; i < MAX_SLOTS; ++i) {
            if (slots_[i].trackingId >= 0 && slots_[i].dirty) {
                float rawX = slots_[i].x;
                if (rawX >= sidebarEvdevX0_ && rawX <= sidebarEvdevX1_) {
                    // Touch is in sidebar — check hit zones on finger DOWN only
                    if (prevSlots_[i].trackingId < 0) {
                        float rawY = slots_[i].y;
                        if (rawY >= sidebarVolUpY0_ && rawY < sidebarVolUpY1_)
                            emit sidebarVolumeUp();
                        else if (rawY >= sidebarVolDownY0_ && rawY < sidebarVolDownY1_)
                            emit sidebarVolumeDown();
                        else if (rawY >= sidebarHomeY0_ && rawY <= sidebarHomeY1_)
                            emit sidebarHome();
                    }
                    slots_[i].dirty = false;  // consume — don't forward to AA
                    anySidebarTouch = true;
                }
            }
        }
        // If ALL touches in this sync are sidebar touches, skip AA processing
        bool anyDirty = false;
        for (int i = 0; i < MAX_SLOTS; ++i)
            if (slots_[i].dirty) { anyDirty = true; break; }
        if (anySidebarTouch && !anyDirty) {
            prevSlots_ = slots_;
            return;
        }
    }

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

void EvdevTouchReader::grab()
{
    if (fd_ < 0 || grabbed_.load()) return;

    if (::ioctl(fd_, EVIOCGRAB, 1) < 0) {
        BOOST_LOG_TRIVIAL(warning) << "[EvdevTouch] EVIOCGRAB failed: " << strerror(errno);
        return;
    }
    grabbed_.store(true);
    BOOST_LOG_TRIVIAL(info) << "[EvdevTouch] Device grabbed — touch events routed to AA";
}

void EvdevTouchReader::ungrab()
{
    if (fd_ < 0 || !grabbed_.load()) return;

    ::ioctl(fd_, EVIOCGRAB, 0);
    grabbed_.store(false);

    // Reset slot state so stale touches don't fire when re-grabbed
    slots_.fill(Slot{});
    prevSlots_ = slots_;
    currentSlot_ = 0;
    gestureActive_ = false;
    gestureMaxFingers_ = 0;
    prevActiveCount_ = 0;

    BOOST_LOG_TRIVIAL(info) << "[EvdevTouch] Device ungrabbed — touch returned to Wayland";
}

} // namespace aa
} // namespace oap
