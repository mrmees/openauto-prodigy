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
    bool horizontal = sidebarHorizontal_;

    int effectiveDisplayW = displayWidth_;
    int effectiveDisplayH = displayHeight_;
    int effectiveDisplayX0 = 0;
    int effectiveDisplayY0 = 0;

    if (sidebarEnabled_ && sidebarPixelWidth_ > 0) {
        if (horizontal) {
            effectiveDisplayH = displayHeight_ - sidebarPixelWidth_;
            if (sidebarPosition_ == "top")
                effectiveDisplayY0 = sidebarPixelWidth_;
        } else {
            effectiveDisplayW = displayWidth_ - sidebarPixelWidth_;
            if (sidebarPosition_ == "left")
                effectiveDisplayX0 = sidebarPixelWidth_;
        }
    }

    float videoAspect = static_cast<float>(aaWidth_) / aaHeight_;
    float displayAspect = static_cast<float>(effectiveDisplayW) / effectiveDisplayH;

    float evdevPerPixelX = static_cast<float>(screenWidth_) / displayWidth_;
    float evdevPerPixelY = static_cast<float>(screenHeight_) / displayHeight_;

    float videoPixelW, videoPixelH, videoPixelX0, videoPixelY0;

    // Reset crop offset defaults
    cropAAOffsetX_ = 0;
    visibleAAWidth_ = aaWidth_;
    cropAAOffsetY_ = 0;
    visibleAAHeight_ = aaHeight_;

    if (sidebarEnabled_ && !horizontal && videoAspect > displayAspect) {
        // X-crop mode (side sidebar): video fills height, X gets cropped
        videoPixelH = effectiveDisplayH;
        float scale = static_cast<float>(effectiveDisplayH) / aaHeight_;
        float totalVideoWidthPx = aaWidth_ * scale;
        float cropPx = (totalVideoWidthPx - effectiveDisplayW) / 2.0f;

        cropAAOffsetX_ = cropPx / scale;
        visibleAAWidth_ = effectiveDisplayW / scale;

        videoPixelW = effectiveDisplayW;
        videoPixelX0 = effectiveDisplayX0;
        videoPixelY0 = effectiveDisplayY0;

        qInfo() << "[EvdevTouch] X-crop mode: video " << aaWidth_ << "x" << aaHeight_
                                << " in " << effectiveDisplayW << "x" << effectiveDisplayH
                                << " | AA visible X: " << cropAAOffsetX_
                                << " to " << (cropAAOffsetX_ + visibleAAWidth_)
                                << " (" << visibleAAWidth_ << " wide)";
    } else if (sidebarEnabled_ && horizontal && displayAspect > videoAspect) {
        // Y-crop mode (top/bottom sidebar): video fills width, Y gets cropped
        videoPixelW = effectiveDisplayW;
        float scale = static_cast<float>(effectiveDisplayW) / aaWidth_;
        float totalVideoHeightPx = aaHeight_ * scale;
        float cropPy = (totalVideoHeightPx - effectiveDisplayH) / 2.0f;

        cropAAOffsetY_ = cropPy / scale;
        visibleAAHeight_ = effectiveDisplayH / scale;

        videoPixelH = effectiveDisplayH;
        videoPixelX0 = effectiveDisplayX0;
        videoPixelY0 = effectiveDisplayY0;

        qInfo() << "[EvdevTouch] Y-crop mode: video " << aaWidth_ << "x" << aaHeight_
                                << " in " << effectiveDisplayW << "x" << effectiveDisplayH
                                << " | AA visible Y: " << cropAAOffsetY_
                                << " to " << (cropAAOffsetY_ + visibleAAHeight_)
                                << " (" << visibleAAHeight_ << " tall)";
    } else if (videoAspect > displayAspect) {
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

    qInfo() << "[EvdevTouch] Mapping: display " << effectiveDisplayW << "x" << effectiveDisplayH
                            << " at pixel (" << videoPixelX0 << "," << videoPixelY0 << ")"
                            << " | evdev (" << videoEvdevX0_ << "," << videoEvdevY0_
                            << ") " << videoEvdevW_ << "x" << videoEvdevH_;
    // Push content dimensions to TouchHandler for debug overlay
    if (handler_)
        handler_->setContentDims(static_cast<int>(visibleAAWidth_), static_cast<int>(visibleAAHeight_));

    qInfo() << "[EvdevTouch] Diagnostic: sidebar=" << (sidebarEnabled_ ? sidebarPosition_.c_str() : "off")
                            << " " << sidebarPixelWidth_ << "px"
                            << " | contentW=" << visibleAAWidth_ << " contentH=" << visibleAAHeight_
                            << " | touch range: X=[" << mapX(static_cast<int>(videoEvdevX0_))
                            << "," << mapX(static_cast<int>(videoEvdevX0_ + videoEvdevW_))
                            << "] Y=[" << mapY(static_cast<int>(videoEvdevY0_))
                            << "," << mapY(static_cast<int>(videoEvdevY0_ + videoEvdevH_)) << "]";
}

void EvdevTouchReader::setSidebar(bool enabled, int width, const std::string& position)
{
    sidebarEnabled_ = enabled;
    sidebarPixelWidth_ = width;
    sidebarPosition_ = position;
    sidebarHorizontal_ = (position == "top" || position == "bottom");

    if (!enabled || width <= 0) return;

    float evdevPerPixelX = static_cast<float>(screenWidth_) / displayWidth_;
    float evdevPerPixelY = static_cast<float>(screenHeight_) / displayHeight_;

    sidebarDragSlot_ = -1;

    if (sidebarHorizontal_) {
        // Horizontal sidebar (top/bottom): Y band, X sub-zones
        if (position == "bottom") {
            int sidebarStartPx = displayHeight_ - width;
            sidebarEvdevY0_ = sidebarStartPx * evdevPerPixelY;
            sidebarEvdevY1_ = screenHeight_;
        } else {
            sidebarEvdevY0_ = 0;
            sidebarEvdevY1_ = width * evdevPerPixelY;
        }
        // Sub-zones along X: volume takes most of the width, small home button at end
        // Matches QML layout where home icon is ~56px at right edge
        sidebarVolX0_ = 0;
        sidebarVolX1_ = (displayWidth_ - 100.0f) * evdevPerPixelX;  // volume up to ~100px from right
        sidebarHomeX0_ = (displayWidth_ - 80.0f) * evdevPerPixelX;  // home zone ~80px at right
        sidebarHomeX1_ = screenWidth_;

        qInfo() << "[EvdevTouch] Sidebar: " << position.c_str() << " " << width << "px"
                                << ", evdev Y: " << sidebarEvdevY0_ << "-" << sidebarEvdevY1_;
    } else {
        // Vertical sidebar (left/right): X band, Y sub-zones
        if (position == "right") {
            int sidebarStartPx = displayWidth_ - width;
            sidebarEvdevX0_ = sidebarStartPx * evdevPerPixelX;
            sidebarEvdevX1_ = screenWidth_;
        } else {
            sidebarEvdevX0_ = 0;
            sidebarEvdevX1_ = width * evdevPerPixelX;
        }
        // Sub-zones along Y: top 70% = volume, gap 5%, bottom 25% = home
        sidebarVolY0_ = 0;
        sidebarVolY1_ = displayHeight_ * 0.70f * evdevPerPixelY;
        sidebarHomeY0_ = displayHeight_ * 0.75f * evdevPerPixelY;
        sidebarHomeY1_ = screenHeight_;

        qInfo() << "[EvdevTouch] Sidebar: " << position.c_str() << " " << width << "px"
                                << ", evdev X: " << sidebarEvdevX0_ << "-" << sidebarEvdevX1_;
    }
}

void EvdevTouchReader::setAAResolution(int aaWidth, int aaHeight)
{
    pendingAAWidth_.store(aaWidth, std::memory_order_relaxed);
    pendingAAHeight_.store(aaHeight, std::memory_order_release);
    qInfo() << "[EvdevTouch] Pending resolution update:" << aaWidth << "x" << aaHeight;
}

int EvdevTouchReader::mapX(int rawX) const
{
    float rel = (rawX - videoEvdevX0_) / videoEvdevW_;
    rel = std::clamp(rel, 0.0f, 1.0f);
    // Map to content coordinate space (0 to visibleAAWidth_).
    // The phone handles margin offset internally — we should NOT add cropAAOffsetX_.
    // touch_screen_config is set to content dimensions to match.
    return static_cast<int>(rel * visibleAAWidth_);
}

int EvdevTouchReader::mapY(int rawY) const
{
    float rel = (rawY - videoEvdevY0_) / videoEvdevH_;
    rel = std::clamp(rel, 0.0f, 1.0f);
    // Map to content coordinate space (0 to visibleAAHeight_).
    // For top/bottom sidebar, visibleAAHeight_ < aaHeight_ (Y-crop active).
    // For side sidebar or no sidebar, visibleAAHeight_ == aaHeight_.
    return static_cast<int>(rel * visibleAAHeight_);
}

void EvdevTouchReader::run()
{
    fd_ = ::open(devicePath_.c_str(), O_RDONLY);
    if (fd_ < 0) {
        qCritical() << "[EvdevTouch] Failed to open " << devicePath_.c_str()
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
            qWarning() << "[EvdevTouch] X axis min=" << absX.minimum
                                       << " (non-zero — coordinate normalization may be off)";
    }
    if (::ioctl(fd_, EVIOCGABS(ABS_MT_POSITION_Y), &absY) == 0) {
        screenHeight_ = absY.maximum;
        if (absY.minimum != 0)
            qWarning() << "[EvdevTouch] Y axis min=" << absY.minimum
                                       << " (non-zero — coordinate normalization may be off)";
    }

    // Recompute letterbox with actual axis ranges
    computeLetterbox();

    qInfo() << "[EvdevTouch] Opened " << devicePath_.c_str()
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
    qInfo() << "[EvdevTouch] Reader thread stopped";
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
            qInfo() << "[EvdevTouch] 3-finger gesture detected (" << ms << "ms)";
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
        qInfo() << "[EvdevTouch] Applied resolution update:" << aaWidth_ << "x" << aaHeight_;
    }

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
                float rawY = slots_[i].y;
                bool inSidebar = false;

                if (sidebarHorizontal_) {
                    // Horizontal sidebar: check Y band
                    inSidebar = (rawY >= sidebarEvdevY0_ && rawY <= sidebarEvdevY1_);
                } else {
                    // Vertical sidebar: check X band
                    inSidebar = (rawX >= sidebarEvdevX0_ && rawX <= sidebarEvdevX1_);
                }

                if (inSidebar) {
                    bool isDown = prevSlots_[i].trackingId < 0;

                    if (sidebarHorizontal_) {
                        // Horizontal: volume along X (left=0%, right=100%), home on right
                        if (isDown) {
                            if (rawX >= sidebarVolX0_ && rawX < sidebarVolX1_) {
                                sidebarDragSlot_ = i;
                                float rel = (rawX - sidebarVolX0_) / (sidebarVolX1_ - sidebarVolX0_);
                                int vol = std::clamp(static_cast<int>(rel * 100), 0, 100);
                                emit sidebarVolumeSet(vol);
                            } else if (rawX >= sidebarHomeX0_ && rawX <= sidebarHomeX1_) {
                                emit sidebarHome();
                            }
                        } else if (i == sidebarDragSlot_) {
                            float rel = (rawX - sidebarVolX0_) / (sidebarVolX1_ - sidebarVolX0_);
                            int vol = std::clamp(static_cast<int>(rel * 100), 0, 100);
                            emit sidebarVolumeSet(vol);
                        }
                    } else {
                        // Vertical: volume along Y (top=100%, bottom=0%), home on bottom
                        if (isDown) {
                            if (rawY >= sidebarVolY0_ && rawY < sidebarVolY1_) {
                                sidebarDragSlot_ = i;
                                float rel = 1.0f - (rawY - sidebarVolY0_) / (sidebarVolY1_ - sidebarVolY0_);
                                int vol = std::clamp(static_cast<int>(rel * 100), 0, 100);
                                emit sidebarVolumeSet(vol);
                            } else if (rawY >= sidebarHomeY0_ && rawY <= sidebarHomeY1_) {
                                emit sidebarHome();
                            }
                        } else if (i == sidebarDragSlot_) {
                            float rel = 1.0f - (rawY - sidebarVolY0_) / (sidebarVolY1_ - sidebarVolY0_);
                            int vol = std::clamp(static_cast<int>(rel * 100), 0, 100);
                            emit sidebarVolumeSet(vol);
                        }
                    }

                    slots_[i].dirty = false;  // consume — don't forward to AA
                    anySidebarTouch = true;
                }
            }
            // End drag when finger lifts
            if (slots_[i].trackingId < 0 && i == sidebarDragSlot_)
                sidebarDragSlot_ = -1;
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

            qInfo() << "[EvdevTouch] DOWN slot=" << i
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

            qInfo() << "[EvdevTouch] UP slot=" << i
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
        qWarning() << "[EvdevTouch] EVIOCGRAB failed: " << strerror(errno);
        return;
    }
    grabbed_.store(true);
    qInfo() << "[EvdevTouch] Device grabbed — touch events routed to AA";
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

    qInfo() << "[EvdevTouch] Device ungrabbed — touch returned to Wayland";
}

} // namespace aa
} // namespace oap
