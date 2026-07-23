#include "EvdevTouchReader.hpp"
#include "../Logging.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <cstring>
#include <algorithm>
#include <QDeadlineTimer>

namespace oap {
namespace aa {

EvdevTouchReader::EvdevTouchReader(TouchHandler* handler,
                                   const std::string& devicePath,
                                   int aaWidth, int aaHeight,
                                   int displayWidth, int displayHeight,
                                   QObject* parent)
    : QThread(parent)
    , handler_(handler)
    , devicePath_(devicePath)
    , aaWidth_(aaWidth)
    , aaHeight_(aaHeight)
    , displayWidth_(displayWidth)
    , displayHeight_(displayHeight)
{
    slots_.fill(Slot{});
    prevSlots_.fill(Slot{});
    aaActive_.fill(false);
    requestedMapping_.aaWidth = aaWidth;
    requestedMapping_.aaHeight = aaHeight;
    requestedMapping_.displayWidth = displayWidth;
    requestedMapping_.displayHeight = displayHeight;
}

void EvdevTouchReader::requestStop()
{
    QMutexLocker lock(&reconnectMutex_);
    stopRequested_.store(true, std::memory_order_release);
    reconnectCondition_.wakeAll();
}

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

    effectiveDisplayW = std::max(1, effectiveDisplayW);
    effectiveDisplayH = std::max(1, effectiveDisplayH);
    displayWidth_ = std::max(1, displayWidth_);
    displayHeight_ = std::max(1, displayHeight_);

    float evdevPerPixelX = static_cast<float>(screenWidth_) / displayWidth_;
    float evdevPerPixelY = static_cast<float>(screenHeight_) / displayHeight_;

    float videoPixelW, videoPixelH, videoPixelX0, videoPixelY0;

    // Use content-space dimensions when set (matches touch_screen_config),
    // otherwise fall back to full video resolution (backward compatible).
    visibleAAWidth_ = (contentWidth_ > 0) ? contentWidth_ : aaWidth_;
    visibleAAHeight_ = (contentHeight_ > 0) ? contentHeight_ : aaHeight_;

    // Use CONTENT aspect ratio, not frame ratio. With PreserveAspectCrop,
    // the video fills the display area and any margin bars are cropped away.
    // Content dimensions already account for navbar via computeContentDimensions.
    float contentAspect = static_cast<float>(visibleAAWidth_) / visibleAAHeight_;
    float displayAspect = static_cast<float>(effectiveDisplayW) / effectiveDisplayH;

    if (contentAspect > displayAspect) {
        // Content fills width, letterbox top/bottom
        videoPixelW = effectiveDisplayW;
        videoPixelH = effectiveDisplayW / contentAspect;
        videoPixelX0 = effectiveDisplayX0;
        videoPixelY0 = effectiveDisplayY0 + (effectiveDisplayH - videoPixelH) / 2.0f;
    } else {
        // Content fills height, letterbox left/right
        videoPixelH = effectiveDisplayH;
        videoPixelW = effectiveDisplayH * contentAspect;
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
    if (handler_) {
        const int contentW = static_cast<int>(visibleAAWidth_);
        const int contentH = static_cast<int>(visibleAAHeight_);
        QMetaObject::invokeMethod(handler_, [handler = handler_, contentW, contentH]() {
            handler->setContentDims(contentW, contentH);
        }, Qt::QueuedConnection);
    }

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
    std::lock_guard<std::mutex> lock(mappingMutex_);
    requestedMapping_.navbarEnabled = enabled;
    requestedMapping_.navbarThickness = thickness;
    requestedMapping_.navbarEdge = edge;
    ++requestedMapping_.version;
}

void EvdevTouchReader::setVideoMapping(int aaWidth, int aaHeight,
                                       int contentWidth, int contentHeight)
{
    std::lock_guard<std::mutex> lock(mappingMutex_);
    requestedMapping_.aaWidth = aaWidth;
    requestedMapping_.aaHeight = aaHeight;
    requestedMapping_.contentWidth = contentWidth;
    requestedMapping_.contentHeight = contentHeight;
    ++requestedMapping_.version;
    qCDebug(lcAA) << "Pending video mapping update: frame="
                  << aaWidth << "x" << aaHeight << "content="
                  << contentWidth << "x" << contentHeight;
}

void EvdevTouchReader::setDisplayDimensions(int w, int h)
{
    std::lock_guard<std::mutex> lock(mappingMutex_);
    requestedMapping_.displayWidth = w;
    requestedMapping_.displayHeight = h;
    ++requestedMapping_.version;
    qCDebug(lcAA) << "Pending display dimension update:" << w << "x" << h;
}

void EvdevTouchReader::applyPendingMapping(bool force)
{
    MappingConfig mapping;
    {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        if (!force && requestedMapping_.version == appliedMappingVersion_)
            return;
        mapping = requestedMapping_;
    }

    aaWidth_ = mapping.aaWidth;
    aaHeight_ = mapping.aaHeight;
    displayWidth_ = mapping.displayWidth;
    displayHeight_ = mapping.displayHeight;
    contentWidth_ = mapping.contentWidth;
    contentHeight_ = mapping.contentHeight;
    navbarEnabled_ = mapping.navbarEnabled;
    navbarThickness_ = mapping.navbarThickness;
    navbarEdge_ = mapping.navbarEdge;
    appliedMappingVersion_ = mapping.version;
    computeLetterbox();
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

bool EvdevTouchReader::openDevice()
{
    fd_ = ::open(devicePath_.c_str(), O_RDONLY | O_CLOEXEC);
    return fd_ >= 0;
}

void EvdevTouchReader::closeDevice()
{
    if (fd_ < 0)
        return;
    if (deviceGrabbed_)
        ::ioctl(fd_, EVIOCGRAB, 0);
    ::close(fd_);
    fd_ = -1;
    deviceGrabbed_ = false;
}

void EvdevTouchReader::queryAxisRanges()
{
    struct input_absinfo absX{}, absY{};
    if (::ioctl(fd_, EVIOCGABS(ABS_MT_POSITION_X), &absX) == 0) {
        screenWidth_ = std::max(1, absX.maximum);
        if (absX.minimum != 0)
            qCWarning(lcAA) << "X axis min=" << absX.minimum
                            << "(non-zero — coordinate normalization may be off)";
    }
    if (::ioctl(fd_, EVIOCGABS(ABS_MT_POSITION_Y), &absY) == 0) {
        screenHeight_ = std::max(1, absY.maximum);
        if (absY.minimum != 0)
            qCWarning(lcAA) << "Y axis min=" << absY.minimum
                            << "(non-zero — coordinate normalization may be off)";
    }
}

int EvdevTouchReader::pollDevice(short& revents, int timeoutMs)
{
    pollfd pfd{};
    pfd.fd = fd_;
    pfd.events = POLLIN;
    const int result = ::poll(&pfd, 1, timeoutMs);
    revents = pfd.revents;
    return result;
}

ssize_t EvdevTouchReader::readDeviceEvent(input_event& event)
{
    return ::read(fd_, &event, sizeof(event));
}

bool EvdevTouchReader::setDeviceGrab(bool grabbed)
{
    return fd_ >= 0 && ::ioctl(fd_, EVIOCGRAB, grabbed ? 1 : 0) == 0;
}

bool EvdevTouchReader::waitForReconnect()
{
    QMutexLocker lock(&reconnectMutex_);
    QDeadlineTimer deadline(1000);
    while (!stopRequested_.load(std::memory_order_acquire)) {
        if (!reconnectCondition_.wait(&reconnectMutex_, deadline))
            return true;
    }
    return false;
}

void EvdevTouchReader::releaseAaTouchStream()
{
    // The AA touch enum has no ACTION_CANCEL. Retire every phone-visible
    // pointer with valid POINTER_UP/UP messages before local ownership or the
    // device disappears, so the phone cannot retain a stuck gesture.
    for (int i = 0; i < MAX_SLOTS; ++i) {
        if (!aaActive_[i])
            continue;
        auto pointers = buildAaPointers();
        const auto changed = std::find_if(pointers.begin(), pointers.end(),
            [i](const TouchHandler::Pointer& pointer) { return pointer.id == i; });
        const int actionIdx = static_cast<int>(std::distance(pointers.begin(), changed));
        const int action = pointers.size() == 1 ? 1 : 6;
        if (handler_)
            handler_->sendTouchIndication(pointers.size(), pointers.data(),
                                          actionIdx, action);
        aaActive_[i] = false;
    }
}

void EvdevTouchReader::resetTouchState()
{
    releaseAaTouchStream();
    slots_.fill(Slot{});
    prevSlots_ = slots_;
    aaActive_.fill(false);
    currentSlot_ = 0;
    gestureActive_ = false;
    gestureMaxFingers_ = 0;
    prevActiveCount_ = 0;
    router_.resetClaims();
}

void EvdevTouchReader::applyRequestedGrab()
{
    const bool requested = requestedGrab_.load(std::memory_order_acquire);
    if (requested == deviceGrabbed_ || fd_ < 0)
        return;

    if (!setDeviceGrab(requested)) {
        qCWarning(lcAA) << "EVIOCGRAB" << (requested ? "grab" : "release")
                        << "failed:" << strerror(errno);
        return;
    }

    deviceGrabbed_ = requested;
    if (!requested)
        resetTouchState();
    qCInfo(lcAA) << (requested
        ? "Device grabbed — touch events routed to AA"
        : "Device ungrabbed — touch returned to Wayland");
}

void EvdevTouchReader::run()
{
    while (!stopRequested_.load(std::memory_order_acquire)) {
        if (!openDevice()) {
            qCWarning(lcAA) << "Failed to open" << devicePath_.c_str()
                            << ":" << strerror(errno) << "— retrying";
            if (!waitForReconnect())
                break;
            continue;
        }

        queryAxisRanges();
        applyPendingMapping(true);
        resetTouchState();
        applyRequestedGrab();

        qCInfo(lcAA) << "Opened" << devicePath_.c_str()
                     << "(evdev:" << screenWidth_ << "x" << screenHeight_
                     << "-> AA" << aaWidth_ << "x" << aaHeight_ << ")";

        bool deviceHealthy = true;
        while (deviceHealthy && !stopRequested_.load(std::memory_order_acquire)) {
            applyPendingMapping();
            applyRequestedGrab();

            short revents = 0;
            const int pollResult = pollDevice(revents, 100);
            if (pollResult == 0)
                continue;
            if (pollResult < 0) {
                if (errno == EINTR)
                    continue;
                qCWarning(lcAA) << "Touch device poll failed:" << strerror(errno);
                deviceHealthy = false;
                break;
            }
            if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
                qCWarning(lcAA) << "Touch device became unavailable, revents="
                                << Qt::hex << revents;
                deviceHealthy = false;
                break;
            }
            if (!(revents & POLLIN))
                continue;

            input_event ev{};
            const ssize_t bytesRead = readDeviceEvent(ev);
            if (bytesRead != static_cast<ssize_t>(sizeof(ev))) {
                if (bytesRead < 0 && (errno == EINTR || errno == EAGAIN))
                    continue;
                qCWarning(lcAA) << "Touch device read failed/short:" << bytesRead
                                << (bytesRead < 0 ? strerror(errno) : "bytes");
                deviceHealthy = false;
                break;
            }

            switch (ev.type) {
            case EV_ABS:
                switch (ev.code) {
                case ABS_MT_SLOT:
                    currentSlot_ = std::clamp(ev.value, 0, MAX_SLOTS - 1);
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
                    if (deviceGrabbed_) {
                        processSync();
                    } else {
                        checkGesture();
                        for (auto& slot : slots_)
                            slot.dirty = false;
                        prevSlots_ = slots_;
                    }
                }
                break;
            }
        }

        closeDevice();
        resetTouchState();
        if (!stopRequested_.load(std::memory_order_acquire)
            && !waitForReconnect()) {
            break;
        }
    }

    closeDevice();
    qCInfo(lcAA) << "Reader thread stopped";
}

int EvdevTouchReader::countActive() const
{
    int n = 0;
    for (auto& s : slots_)
        if (s.trackingId >= 0) ++n;
    return n;
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

std::vector<TouchHandler::Pointer> EvdevTouchReader::buildAaPointers() const
{
    std::vector<TouchHandler::Pointer> pointers;
    pointers.reserve(MAX_SLOTS);
    for (int i = 0; i < MAX_SLOTS; ++i) {
        if (!aaActive_[i])
            continue;

        const Slot& source = slots_[i].trackingId >= 0 ? slots_[i] : prevSlots_[i];
        pointers.push_back({mapX(source.x), mapY(source.y), i});
    }
    return pointers;
}

void EvdevTouchReader::processSync()
{
    applyPendingMapping();

    // Check for 3-finger gesture — may suppress AA forwarding but not zone dispatch
    const bool gestureBlocking = checkGesture();

    std::array<bool, MAX_SLOTS> consumed{};

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

        consumed[i] = router_.dispatch(i, x, y, evt);
    }

    // If gesture is active, suppress AA forwarding for unclaimed touches
    if (gestureBlocking) {
        releaseAaTouchStream();
        aaActive_.fill(false);
        for (int i = 0; i < MAX_SLOTS; ++i)
            slots_[i].dirty = false;
        prevSlots_ = slots_;
        return;
    }

    bool sentBoundaryEvent = false;

    // Retire phone-visible pointers before admitting replacements from the
    // same SYN_REPORT. Android must not observe a transient overlap between a
    // lifted contact and a newly pressed contact.
    for (int i = 0; i < MAX_SLOTS; ++i) {
        if (!slots_[i].dirty || consumed[i] || !aaActive_[i])
            continue;

        const bool wasActive = prevSlots_[i].trackingId >= 0;
        const bool isActive = slots_[i].trackingId >= 0;
        if (!wasActive || isActive)
            continue;

        auto pointers = buildAaPointers();
        const auto changed = std::find_if(pointers.begin(), pointers.end(),
            [i](const TouchHandler::Pointer& pointer) { return pointer.id == i; });
        const int actionIdx = static_cast<int>(std::distance(pointers.begin(), changed));
        const int action = pointers.size() == 1 ? 1 : 6;
        if (handler_) {
            handler_->sendTouchIndication(pointers.size(), pointers.data(),
                                          actionIdx, action);
        }
        aaActive_[i] = false;
        sentBoundaryEvent = true;
        qCDebug(lcAA) << "UP slot=" << i << "actionIdx=" << actionIdx
                      << "active=" << pointers.size();
    }

    // Admit unclaimed DOWN transitions one at a time. This preserves Android's
    // required DOWN then POINTER_DOWN ordering even when evdev batches them in
    // the same SYN_REPORT.
    for (int i = 0; i < MAX_SLOTS; ++i) {
        if (!slots_[i].dirty || consumed[i])
            continue;

        const bool wasActive = prevSlots_[i].trackingId >= 0;
        const bool isActive = slots_[i].trackingId >= 0;
        if (wasActive || !isActive)
            continue;

        aaActive_[i] = true;
        auto pointers = buildAaPointers();
        const auto changed = std::find_if(pointers.begin(), pointers.end(),
            [i](const TouchHandler::Pointer& pointer) { return pointer.id == i; });
        const int actionIdx = static_cast<int>(std::distance(pointers.begin(), changed));
        const int action = pointers.size() == 1 ? 0 : 5;

        if (handler_) {
            handler_->sendTouchIndication(pointers.size(), pointers.data(),
                                          actionIdx, action);
        }
        sentBoundaryEvent = true;
        qCDebug(lcAA) << "DOWN slot=" << i << "actionIdx=" << actionIdx
                      << "active=" << pointers.size();
    }

    // A boundary message already carries the current coordinates for every
    // phone-visible pointer, so MOVE is only needed for a pure move sync.
    bool anyMoved = false;
    for (int i = 0; i < MAX_SLOTS; ++i) {
        if (slots_[i].dirty && !consumed[i] && aaActive_[i]
            && slots_[i].trackingId >= 0 && prevSlots_[i].trackingId >= 0) {
            if (slots_[i].x != prevSlots_[i].x || slots_[i].y != prevSlots_[i].y) {
                anyMoved = true;
                break;
            }
        }
    }

    if (anyMoved && !sentBoundaryEvent) {
        auto pointers = buildAaPointers();
        if (handler_ && !pointers.empty())
            handler_->sendTouchIndication(pointers.size(), pointers.data(), 0, 2);
    }

    // Clear dirty flags and save state
    for (int i = 0; i < MAX_SLOTS; ++i)
        slots_[i].dirty = false;
    prevSlots_ = slots_;
}

void EvdevTouchReader::grab()
{
    requestedGrab_.store(true, std::memory_order_release);
}

void EvdevTouchReader::ungrab()
{
    requestedGrab_.store(false, std::memory_order_release);
}

} // namespace aa
} // namespace oap
