#pragma once

#include <QThread>
#include <array>
#include <atomic>
#include <vector>
#include <string>
#include <chrono>
#include <linux/input.h>
#include <QDebug>
#include "TouchHandler.hpp"
#include "TouchRouter.hpp"

namespace oap {
namespace aa {

class EvdevCoordBridge;  // forward declaration

// Reads multi-touch events directly from a Linux evdev device (MT Type B protocol).
// Bypasses Qt's touch input entirely to avoid Wayland/evdev plugin conflicts.
// Tracks full slot state and sends properly formed AA touch messages.
//
// Supports letterboxed video: if the AA video aspect ratio doesn't match the
// physical display, touches in the black bar area are clamped to the video edge.
//
// 3-finger gesture: When 3 simultaneous touches are detected within a 200ms
// window, emits gestureDetected() and suppresses those touches from AA.
class EvdevTouchReader : public QThread {
    Q_OBJECT

public:
    static constexpr int MAX_SLOTS = 10;
    static constexpr int GESTURE_FINGER_COUNT = 3;
    static constexpr int GESTURE_WINDOW_MS = 200;

    struct Slot {
        int trackingId = -1;  // -1 = inactive
        int x = 0;
        int y = 0;
        bool dirty = false;   // changed since last SYN
    };

    explicit EvdevTouchReader(TouchHandler* handler,
                              const std::string& devicePath,
                              int aaWidth, int aaHeight,
                              int displayWidth, int displayHeight,
                              QObject* parent = nullptr)
        : QThread(parent)
        , handler_(handler)
        , devicePath_(devicePath)
        , aaWidth_(aaWidth)
        , aaHeight_(aaHeight)
        , displayWidth_(displayWidth)
        , displayHeight_(displayHeight)
    {
        slots_.fill(Slot{});
    }

    void requestStop() { stopRequested_ = true; }

    /// Grab/ungrab the evdev device. When ungrabbed, events are discarded and
    /// Wayland/libinput can handle the touch device for normal UI interaction.
    void grab();
    void ungrab();

    void setNavbar(bool enabled, int thickness, const std::string& edge);
    void setCoordBridge(EvdevCoordBridge* bridge) { coordBridge_ = bridge; }
    void computeLetterbox();

    /// Direct access to the touch router (for external zone registration)
    TouchRouter& router() { return router_; }

    /// Set content-space dimensions (video res minus margins).
    /// When set, mapX/mapY output in this coordinate space instead of full video res.
    /// Must match touch_screen_config sent to the phone.
    void setContentDimensions(int w, int h);

    /// Thread-safe: update AA coordinate space (e.g. after video resolution change).
    /// Takes effect on next touch sync on the reader thread.
    void setAAResolution(int aaWidth, int aaHeight);

    /// Thread-safe: update display dimensions (e.g. after window resize detection).
    /// Takes effect on next touch sync on the reader thread.
    void setDisplayDimensions(int w, int h);

signals:
    /// Emitted when a 3-finger tap gesture is detected (thread-safe, queued).
    void gestureDetected();

protected:
    void run() override;

private:
    void processSync();
    int countActive() const;
    int slotToArrayIndex(int slot) const;
    bool checkGesture();

    // Map raw evdev coordinate to AA coordinate, accounting for letterbox
    int mapX(int rawX) const;
    int mapY(int rawY) const;

    TouchHandler* handler_;
    std::string devicePath_;
    int screenWidth_ = 4095;   // evdev axis max, read from device in run()
    int screenHeight_ = 4095;  // evdev axis max, read from device in run()
    int aaWidth_;       // AA touch coordinate space (video resolution)
    int aaHeight_;
    int displayWidth_;  // physical display pixels
    int displayHeight_;

    // Letterbox: video area within evdev coordinate space
    float videoEvdevX0_ = 0;  // left edge of video in evdev coords
    float videoEvdevY0_ = 0;  // top edge of video in evdev coords
    float videoEvdevW_ = 0;   // width of video area in evdev coords
    float videoEvdevH_ = 0;   // height of video area in evdev coords

    // Visible AA dimensions (always full res with navbar fit-mode)
    float visibleAAWidth_ = 0;
    float visibleAAHeight_ = 0;

    std::array<Slot, MAX_SLOTS> slots_;
    std::array<Slot, MAX_SLOTS> prevSlots_;  // state before this SYN
    int currentSlot_ = 0;
    bool stopRequested_ = false;
    std::atomic<bool> grabbed_{false};
    int fd_ = -1;

    // Gesture detection state
    bool gestureActive_ = false;  // suppressing touches during gesture
    int gestureMaxFingers_ = 0;   // max simultaneous fingers in current gesture window
    std::chrono::steady_clock::time_point firstFingerTime_;
    int prevActiveCount_ = 0;

    // Content-space dimensions (video res minus margins, matches touch_screen_config)
    // When > 0, mapX/mapY output in this coordinate space instead of full video res.
    int contentWidth_ = 0;
    int contentHeight_ = 0;

    // Navbar dimensions for letterbox calculation
    bool navbarEnabled_ = false;
    int navbarThickness_ = 0;
    std::string navbarEdge_ = "bottom";

    // Touch routing
    TouchRouter router_;
    EvdevCoordBridge* coordBridge_ = nullptr;

    // Pending resolution update (set from main thread, consumed on reader thread)
    std::atomic<int> pendingAAWidth_{0};
    std::atomic<int> pendingAAHeight_{0};

    // Pending display dimension update (set from main thread, consumed on reader thread)
    std::atomic<int> pendingDisplayWidth_{0};
    std::atomic<int> pendingDisplayHeight_{0};
};

} // namespace aa
} // namespace oap
