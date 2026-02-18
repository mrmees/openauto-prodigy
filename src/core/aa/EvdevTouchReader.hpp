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
                              int aaWidth = 1024, int aaHeight = 600,
                              QObject* parent = nullptr)
        : QThread(parent)
        , handler_(handler)
        , devicePath_(devicePath)
        , screenWidth_(screenWidth)
        , screenHeight_(screenHeight)
        , aaWidth_(aaWidth)
        , aaHeight_(aaHeight)
    {
        slots_.fill(Slot{});
    }

    void requestStop() { stopRequested_ = true; }

protected:
    void run() override;

private:
    void processSync();
    int countActive() const;
    int slotToArrayIndex(int slot) const;

    TouchHandler* handler_;
    std::string devicePath_;
    int screenWidth_;   // evdev axis max (e.g. 4095)
    int screenHeight_;  // evdev axis max (e.g. 4095)
    int aaWidth_;       // AA touch coordinate space
    int aaHeight_;      // AA touch coordinate space

    std::array<Slot, MAX_SLOTS> slots_;
    std::array<Slot, MAX_SLOTS> prevSlots_;  // state before this SYN
    int currentSlot_ = 0;
    bool stopRequested_ = false;
    int fd_ = -1;
};

} // namespace aa
} // namespace oap
