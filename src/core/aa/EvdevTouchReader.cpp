#include "EvdevTouchReader.hpp"
#include "../Logging.hpp"
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
    int effectiveDisplayH = displayHeight_;
    int effectiveDisplayX0 = 0;
    int effectiveDisplayY0 = 0;

    if (navbarEnabled_ && navbarThickness_ > 0) {
        bool horizontal = (navbarEdge_ == "top" || navbarEdge_ == "bottom");
        if (horizontal) {
            effectiveDisplayH = displayHeight_ - navbarThickness_;
            if (navbarEdge_ == "top")
                effectiveDisplayY0 = navbarThickness_;
        } else {
            effectiveDisplayW = displayWidth_ - navbarThickness_;
            if (navbarEdge_ == "left")
                effectiveDisplayX0 = navbarThickness_;
        }
    }

    float videoAspect = static_cast<float>(aaWidth_) / aaHeight_;
    float displayAspect = static_cast<float>(effectiveDisplayW) / effectiveDisplayH;

    float evdevPerPixelX = static_cast<float>(screenWidth_) / displayWidth_;
    float evdevPerPixelY = static_cast<float>(screenHeight_) / displayHeight_;

    float videoPixelW, videoPixelH, videoPixelX0, videoPixelY0;

    // Use content-space dimensions when set (matches touch_screen_config),
    // otherwise fall back to full video resolution (backward compatible).
    visibleAAWidth_ = (contentWidth_ > 0) ? contentWidth_ : aaWidth_;
    visibleAAHeight_ = (contentHeight_ > 0) ? contentHeight_ : aaHeight_;

    if (videoAspect > displayAspect) {
        // Fit mode: video fills width, letterbox top/bottom
        videoPixelW = effectiveDisplayW;
        videoPixelH = effectiveDisplayW / videoAspect;
        videoPixelX0 = effectiveDisplayX0;
        videoPixelY0 = effectiveDisplayY0 + (effectiveDisplayH - videoPixelH) / 2.0f;
    } else {
        // Fit mode: video fills height, letterbox left/right
        videoPixelH = effectiveDisplayH;
        videoPixelW = effectiveDisplayH * videoAspect;
        videoPixelX0 = effectiveDisplayX0 + (effectiveDisplayW - videoPixelW) / 2.0f;
        videoPixelY0 = effectiveDisplayY0;
    }

    videoEvdevX0_ = videoPixelX0 * evdevPerPixelX;
    videoEvdevY0_ = videoPixelY0 * evdevPerPixelY;
    videoEvdevW_ = videoPixelW * evdevPerPixelX;
    videoEvdevH_ = videoPixelH * evdevPerPixelY;

    qCDebug(lcAA) << "Mapping: display " << effectiveDisplayW << "x" << effectiveDisplayH
                            << " at pixel (" << videoPixelX0 << "," << videoPixelY0 << ")"
                            << " | evdev (" << videoEvdevX0_ << "," << videoEvdevY0_
                            << ") " << videoEvdevW_ << "x" << videoEvdevH_;
    // Push content dimensions to TouchHandler for debug overlay
    if (handler_)
        handler_->setContentDims(static_cast<int>(visibleAAWidth_), static_cast<int>(visibleAAHeight_));

    qCDebug(lcAA) << "Diagnostic: navbar=" << (navbarEnabled_ ? navbarEdge_.c_str() : "off")
                            << " " << navbarThickness_ << "px"
                            << " | video=" << aaWidth_ << "x" << aaHeight_
                            << " content=" << visibleAAWidth_ << "x" << visibleAAHeight_
                            << " | touch range: X=[" << mapX(static_cast<int>(videoEvdevX0_))
                            << "," << mapX(static_cast<int>(videoEvdevX0_ + videoEvdevW_))
                            << "] Y=[" << mapY(static_cast<int>(videoEvdevY0_))
                            << "," << mapY(static_cast<int>(videoEvdevY0_ + videoEvdevH_)) << "]";
}

void EvdevTouchReader::setNavbar(bool enabled, int thickness, const std::string& edge)
{
    navbarEnabled_ = enabled;
    navbarThickness_ = thickness;
    navbarEdge_ = edge;
    // Navbar zones are registered by NavbarController via EvdevCoordBridge -- not here.
}

void EvdevTouchReader::setContentDimensions(int w, int h)
{
    contentWidth_ = w;
    contentHeight_ = h;
    qCInfo(lcAA) << "Content dimensions set:" << w << "x" << h;
}

void EvdevTouchReader::setAAResolution(int aaWidth, int aaHeight)
{
    pendingAAWidth_.store(aaWidth, std::memory_order_relaxed);
    pendingAAHeight_.store(aaHeight, std::memory_order_release);
    qCDebug(lcAA) << "Pending resolution update:" << aaWidth << "x" << aaHeight;
}

void EvdevTouchReader::setDisplayDimensions(int w, int h)
{
    pendingDisplayWidth_.store(w, std::memory_order_relaxed);
    pendingDisplayHeight_.store(h, std::memory_order_release);
    qCDebug(lcAA) << "Pending display dimension update:" << w << "x" << h;
}

int EvdevTouchReader::mapX(int rawX) const
{
    float rel = (rawX - videoEvdevX0_) / videoEvdevW_;
    rel = std::clamp(rel, 0.0f, 1.0f);
    // Map to content coordinate space (0 to visibleAAWidth_).
    // With navbar fit-mode, visibleAAWidth_ == aaWidth_ (no crop).
    return static_cast<int>(rel * visibleAAWidth_);
}

int EvdevTouchReader::mapY(int rawY) const
{
    float rel = (rawY - videoEvdevY0_) / videoEvdevH_;
    rel = std::clamp(rel, 0.0f, 1.0f);
    // Map to content coordinate space (0 to visibleAAHeight_).
    // With navbar fit-mode, visibleAAHeight_ == aaHeight_ (no crop).
    return static_cast<int>(rel * visibleAAHeight_);
}

void EvdevTouchReader::run()
{
    fd_ = ::open(devicePath_.c_str(), O_RDONLY);
    if (fd_ < 0) {
        qCCritical(lcAA) << "Failed to open " << devicePath_.c_str()
                                 << ": " << strerror(errno);
        return;
    }

    // Don't grab on startup — let Wayland/libinput handle touch for the launcher UI.
    // The grab will be activated when AA connects via grab().

    // Read axis ranges for coordinate scaling
    struct input_absinfo absX{}, absY{};
    if (::ioctl(fd_, EVIOCGABS(ABS_MT_POSITION_X), &absX) == 0) {
        screenWidth_ = absX.maximum;
        if (absX.minimum != 0)
            qCWarning(lcAA) << "X axis min=" << absX.minimum
                                       << " (non-zero — coordinate normalization may be off)";
    }
    if (::ioctl(fd_, EVIOCGABS(ABS_MT_POSITION_Y), &absY) == 0) {
        screenHeight_ = absY.maximum;
        if (absY.minimum != 0)
            qCWarning(lcAA) << "Y axis min=" << absY.minimum
                                       << " (non-zero — coordinate normalization may be off)";
    }

    // Recompute letterbox with actual axis ranges
    computeLetterbox();

    qCInfo(lcAA) << "Opened " << devicePath_.c_str()
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

        bool isGrabbed = grabbed_.load(std::memory_order_relaxed);

        // Always process slot tracking — needed for gesture detection even when ungrabbed
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
            if (ev.code == SYN_REPORT) {
                if (isGrabbed) {
                    processSync();  // Full processing: gesture + AA forwarding
                } else {
                    // Ungrabbed: only run gesture detection, don't forward to AA
                    checkGesture();
                    for (int i = 0; i < MAX_SLOTS; ++i)
                        slots_[i].dirty = false;
                    prevSlots_ = slots_;
                }
            }
            break;
        }
    }

    if (grabbed_.load())
        ::ioctl(fd_, EVIOCGRAB, 0);
    ::close(fd_);
    fd_ = -1;
    qCInfo(lcAA) << "Reader thread stopped";
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
            qCInfo(lcAA) << "3-finger gesture detected (" << ms << "ms)";
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
    // Apply pending AA resolution update (set from main thread via setAAResolution)
    int newH = pendingAAHeight_.load(std::memory_order_acquire);
    if (newH > 0) {
        int newW = pendingAAWidth_.load(std::memory_order_relaxed);
        aaWidth_ = newW;
        aaHeight_ = newH;
        pendingAAWidth_.store(0, std::memory_order_relaxed);
        pendingAAHeight_.store(0, std::memory_order_relaxed);
        computeLetterbox();
        qCDebug(lcAA) << "Applied resolution update:" << aaWidth_ << "x" << aaHeight_;
    }

    // Apply pending display dimension update (set from main thread via setDisplayDimensions)
    int newDisplayH = pendingDisplayHeight_.load(std::memory_order_acquire);
    if (newDisplayH > 0) {
        int newDisplayW = pendingDisplayWidth_.load(std::memory_order_relaxed);
        displayWidth_ = newDisplayW;
        displayHeight_ = newDisplayH;
        pendingDisplayWidth_.store(0, std::memory_order_relaxed);
        pendingDisplayHeight_.store(0, std::memory_order_relaxed);
        computeLetterbox();
        qCDebug(lcAA) << "Applied display dimension update:" << displayWidth_ << "x" << displayHeight_;
    }

    // Check for 3-finger gesture — may suppress AA forwarding but not zone dispatch
    bool gestureBlocking = checkGesture();

    // Dispatch touches through TouchRouter — zones claim slots, unclaimed fall through to AA
    for (int i = 0; i < MAX_SLOTS; ++i) {
        if (!slots_[i].dirty) continue;

        bool wasActive = prevSlots_[i].trackingId >= 0;
        bool isActive = slots_[i].trackingId >= 0;

        TouchEvent evt;
        float x, y;
        if (!wasActive && isActive) {
            evt = TouchEvent::Down;
            x = slots_[i].x; y = slots_[i].y;
        } else if (wasActive && isActive) {
            evt = TouchEvent::Move;
            x = slots_[i].x; y = slots_[i].y;
        } else if (wasActive && !isActive) {
            evt = TouchEvent::Up;
            x = prevSlots_[i].x; y = prevSlots_[i].y;
        } else {
            continue;  // inactive->inactive, skip
        }

        if (router_.dispatch(i, x, y, evt)) {
            slots_[i].dirty = false;  // consumed by zone
        }
    }

    // If gesture is active, suppress AA forwarding for unclaimed touches
    if (gestureBlocking) {
        for (int i = 0; i < MAX_SLOTS; ++i)
            slots_[i].dirty = false;
        prevSlots_ = slots_;
        return;
    }

    // If all dirty slots were consumed by zones, skip AA processing
    {
        bool anyDirty = false;
        for (int i = 0; i < MAX_SLOTS; ++i)
            if (slots_[i].dirty) { anyDirty = true; break; }
        if (!anyDirty) {
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

            qCDebug(lcAA) << "DOWN slot=" << i
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

            qCDebug(lcAA) << "UP slot=" << i
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
        qCWarning(lcAA) << "EVIOCGRAB failed: " << strerror(errno);
        return;
    }
    grabbed_.store(true);
    qCInfo(lcAA) << "Device grabbed — touch events routed to AA";
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
    router_.resetClaims();

    qCInfo(lcAA) << "Device ungrabbed — touch returned to Wayland";
}

} // namespace aa
} // namespace oap
