# Android Auto Wireless Performance Optimization — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Make AA video streaming and touch input feel as responsive as a commercial head unit, guided by measured data.

**Architecture:** Instrument the hot paths (video pipeline, touch pipeline), then fix bottlenecks in priority order: video frame copy elimination, decode off main thread, touch event batching, configurable 60fps. Each optimization is isolated to 1-2 files with clean boundaries. Re-measure after each change.

**Tech Stack:** C++17, Qt 6 (QVideoFrame/QVideoSink), FFmpeg (libavcodec), Boost.ASIO, Protobuf, CMake

---

### Task 1: Add PerfStats Utility Struct

**Files:**
- Create: `src/core/aa/PerfStats.hpp`

**Context:** All instrumentation in Tasks 2-3 depends on this. It's a zero-allocation rolling stats accumulator — no heap, no threads, just `steady_clock::now()` calls and arithmetic.

**Step 1: Create the PerfStats header**

```cpp
#pragma once

#include <chrono>
#include <cstdint>
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>
#include <boost/log/trivial.hpp>

namespace oap {
namespace aa {

struct PerfStats {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    struct Metric {
        double sum = 0.0;
        double min = 1e9;
        double max = 0.0;
        uint64_t count = 0;

        void record(double ms) {
            sum += ms;
            min = std::min(min, ms);
            max = std::max(max, ms);
            ++count;
        }

        double avg() const { return count > 0 ? sum / count : 0.0; }

        void reset() {
            sum = 0.0;
            min = 1e9;
            max = 0.0;
            count = 0;
        }
    };

    static double msElapsed(TimePoint start, TimePoint end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};

} // namespace aa
} // namespace oap
```

**Step 2: Verify it compiles**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && cmake --build . -j$(nproc)`
Expected: Clean compile (header-only, included by nothing yet)

Note: The header will only be compiled when included by Tasks 2 and 3. For now just verify cmake configures without errors.

**Step 3: Commit**

```bash
git add src/core/aa/PerfStats.hpp
git commit -m "feat(perf): add PerfStats utility for pipeline instrumentation"
```

---

### Task 2: Instrument Video Pipeline

**Files:**
- Modify: `src/core/aa/VideoDecoder.hpp`
- Modify: `src/core/aa/VideoDecoder.cpp`
- Modify: `src/core/aa/VideoService.cpp`

**Context:** We need timing at 5 checkpoints: enqueue (ASIO thread), decode start (Qt main thread picks up), decode done (after `avcodec_receive_frame`), copy done (after YUV plane copy), display (after `setVideoFrame`). Log rolling stats every 5 seconds.

**Step 1: Add timing members to VideoDecoder.hpp**

Add these includes and members to `VideoDecoder.hpp`:

After `#include <QVideoFrameFormat>` (line 7), add:
```cpp
#include "PerfStats.hpp"
```

After `uint64_t frameCount_ = 0;` (line 48), add:
```cpp
    // Performance instrumentation
    PerfStats::Metric metricQueue_;    // signal emit → decode start
    PerfStats::Metric metricDecode_;   // decode start → avcodec_receive_frame
    PerfStats::Metric metricCopy_;     // receive_frame → copy done
    PerfStats::Metric metricTotal_;    // signal emit → setVideoFrame
    PerfStats::TimePoint lastLogTime_ = PerfStats::Clock::now();
    uint64_t framesSinceLog_ = 0;
    static constexpr double LOG_INTERVAL_SEC = 5.0;
```

Change the `decodeFrame` slot signature from:
```cpp
void decodeFrame(QByteArray h264Data);
```
to:
```cpp
void decodeFrame(QByteArray h264Data, qint64 enqueueTimeNs = 0);
```

**Step 2: Add timestamp to VideoService signal and emit**

In `VideoService.hpp`, change the signal from:
```cpp
void videoFrameData(QByteArray data);
```
to:
```cpp
void videoFrameData(QByteArray data, qint64 enqueueTimeNs);
```

In `VideoService.cpp`, change both `emit` calls.

In `onAVMediaWithTimestampIndication` (line 142), change:
```cpp
emit videoFrameData(QByteArray(reinterpret_cast<const char*>(buffer.cdata), buffer.size));
```
to:
```cpp
emit videoFrameData(QByteArray(reinterpret_cast<const char*>(buffer.cdata), buffer.size),
                    std::chrono::steady_clock::now().time_since_epoch().count());
```

In `onAVMediaIndication` (line 151), change the same way:
```cpp
emit videoFrameData(QByteArray(reinterpret_cast<const char*>(buffer.cdata), buffer.size),
                    std::chrono::steady_clock::now().time_since_epoch().count());
```

**Step 3: Add timing instrumentation to VideoDecoder::decodeFrame**

In `VideoDecoder.cpp`, replace the entire `decodeFrame` method body with timing instrumentation:

```cpp
void VideoDecoder::decodeFrame(QByteArray h264Data, qint64 enqueueTimeNs)
{
    if (!codecCtx_ || !parser_ || !packet_ || !frame_) return;

    auto t_decodeStart = PerfStats::Clock::now();

    const uint8_t* data = reinterpret_cast<const uint8_t*>(h264Data.constData());
    int dataSize = h264Data.size();

    while (dataSize > 0) {
        int consumed = av_parser_parse2(
            parser_, codecCtx_,
            &packet_->data, &packet_->size,
            data, dataSize,
            AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

        if (consumed < 0) {
            BOOST_LOG_TRIVIAL(error) << "[VideoDecoder] Parse error";
            break;
        }

        data += consumed;
        dataSize -= consumed;

        if (packet_->size == 0) continue;

        int ret = avcodec_send_packet(codecCtx_, packet_);
        if (ret < 0) {
            if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                BOOST_LOG_TRIVIAL(warning) << "[VideoDecoder] Send packet error: " << ret;
            }
            continue;
        }

        while (true) {
            ret = avcodec_receive_frame(codecCtx_, frame_);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) {
                BOOST_LOG_TRIVIAL(warning) << "[VideoDecoder] Receive frame error: " << ret;
                break;
            }

            auto t_decodeDone = PerfStats::Clock::now();

            if (videoSink_ && frame_->format == AV_PIX_FMT_YUV420P) {
                QVideoFrameFormat format(
                    QSize(frame_->width, frame_->height),
                    QVideoFrameFormat::Format_YUV420P);

                QVideoFrame videoFrame(format);
                if (videoFrame.map(QVideoFrame::WriteOnly)) {
                    const int yStride = videoFrame.bytesPerLine(0);
                    const uint8_t* ySrc = frame_->data[0];
                    uint8_t* yDst = videoFrame.bits(0);
                    for (int y = 0; y < frame_->height; ++y) {
                        std::memcpy(yDst + y * yStride, ySrc + y * frame_->linesize[0],
                                    std::min(yStride, frame_->linesize[0]));
                    }

                    const int uStride = videoFrame.bytesPerLine(1);
                    const uint8_t* uSrc = frame_->data[1];
                    uint8_t* uDst = videoFrame.bits(1);
                    const int chromaHeight = frame_->height / 2;
                    for (int y = 0; y < chromaHeight; ++y) {
                        std::memcpy(uDst + y * uStride, uSrc + y * frame_->linesize[1],
                                    std::min(uStride, frame_->linesize[1]));
                    }

                    const int vStride = videoFrame.bytesPerLine(2);
                    const uint8_t* vSrc = frame_->data[2];
                    uint8_t* vDst = videoFrame.bits(2);
                    for (int y = 0; y < chromaHeight; ++y) {
                        std::memcpy(vDst + y * vStride, vSrc + y * frame_->linesize[2],
                                    std::min(vStride, frame_->linesize[2]));
                    }

                    videoFrame.unmap();

                    auto t_copyDone = PerfStats::Clock::now();

                    videoSink_->setVideoFrame(videoFrame);

                    auto t_display = PerfStats::Clock::now();

                    // Record timing metrics
                    if (enqueueTimeNs > 0) {
                        auto t_enqueue = PerfStats::TimePoint(
                            std::chrono::nanoseconds(enqueueTimeNs));
                        metricQueue_.record(PerfStats::msElapsed(t_enqueue, t_decodeStart));
                    }
                    metricDecode_.record(PerfStats::msElapsed(t_decodeStart, t_decodeDone));
                    metricCopy_.record(PerfStats::msElapsed(t_decodeDone, t_copyDone));
                    if (enqueueTimeNs > 0) {
                        auto t_enqueue = PerfStats::TimePoint(
                            std::chrono::nanoseconds(enqueueTimeNs));
                        metricTotal_.record(PerfStats::msElapsed(t_enqueue, t_display));
                    }
                    ++framesSinceLog_;
                }

                ++frameCount_;
                if (frameCount_ == 1) {
                    BOOST_LOG_TRIVIAL(info) << "[VideoDecoder] First frame decoded: "
                                            << frame_->width << "x" << frame_->height;
                }

                // Log rolling stats every LOG_INTERVAL_SEC seconds
                auto now = PerfStats::Clock::now();
                double secSinceLog = PerfStats::msElapsed(lastLogTime_, now) / 1000.0;
                if (secSinceLog >= LOG_INTERVAL_SEC) {
                    double fps = framesSinceLog_ / secSinceLog;
                    BOOST_LOG_TRIVIAL(info)
                        << "[Perf] Video: queue=" << std::fixed << std::setprecision(1)
                        << metricQueue_.avg() << "ms"
                        << " decode=" << metricDecode_.avg() << "ms"
                        << " copy=" << metricCopy_.avg() << "ms"
                        << " total=" << metricTotal_.avg() << "ms"
                        << " (p99≈" << metricTotal_.max << "ms)"
                        << " | " << std::setprecision(1) << fps << " fps";
                    metricQueue_.reset();
                    metricDecode_.reset();
                    metricCopy_.reset();
                    metricTotal_.reset();
                    framesSinceLog_ = 0;
                    lastLogTime_ = now;
                }
            }

            av_frame_unref(frame_);
        }
    }
}
```

**Step 4: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`
Expected: Clean compile

**Step 5: Commit**

```bash
git add src/core/aa/PerfStats.hpp src/core/aa/VideoDecoder.hpp src/core/aa/VideoDecoder.cpp src/core/aa/VideoService.hpp src/core/aa/VideoService.cpp
git commit -m "feat(perf): instrument video pipeline with timing metrics"
```

---

### Task 3: Instrument Touch Pipeline

**Files:**
- Modify: `src/core/aa/TouchHandler.hpp`

**Context:** Measure QML→ASIO strand dispatch time and protobuf send time. Log rolling stats every 5 seconds.

**Step 1: Add timing instrumentation to TouchHandler**

Replace the entire `TouchHandler.hpp` with this version that adds timing:

```cpp
#pragma once

#include <QObject>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <iomanip>
#include <aasdk/Channel/Input/InputServiceChannel.hpp>
#include <aasdk/Channel/Promise.hpp>
#include <aasdk_proto/InputEventIndicationMessage.pb.h>
#include <aasdk_proto/TouchEventData.pb.h>
#include <aasdk_proto/TouchLocationData.pb.h>
#include <aasdk_proto/TouchActionEnum.pb.h>

#include "PerfStats.hpp"

namespace oap {
namespace aa {

class TouchHandler : public QObject {
    Q_OBJECT

public:
    explicit TouchHandler(QObject* parent = nullptr)
        : QObject(parent) {}

    void setChannel(std::shared_ptr<aasdk::channel::input::InputServiceChannel> channel,
                    boost::asio::io_service::strand* strand) {
        channel_ = std::move(channel);
        strand_ = strand;
    }

    Q_INVOKABLE void sendTouchEvent(int x, int y, int action) {
        sendMultiTouchEvent(x, y, 0, action);
    }

    Q_INVOKABLE void sendMultiTouchEvent(int x, int y, int pointerId, int action) {
        if (!channel_ || !strand_) return;

        auto t_qml = PerfStats::Clock::now().time_since_epoch().count();

        strand_->dispatch([this, x, y, pointerId, action, t_qml]() {
            auto t_dispatch = PerfStats::Clock::now();
            auto t_qmlPoint = PerfStats::TimePoint(
                std::chrono::nanoseconds(t_qml));

            aasdk::proto::messages::InputEventIndication indication;
            indication.set_timestamp(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            indication.set_disp_channel(0);

            auto* touchEvent = indication.mutable_touch_event();
            touchEvent->set_touch_action(
                static_cast<aasdk::proto::enums::TouchAction::Enum>(action));
            touchEvent->set_action_index(pointerId);

            auto* location = touchEvent->add_touch_location();
            location->set_x(x);
            location->set_y(y);
            location->set_pointer_id(pointerId);

            auto promise = aasdk::channel::SendPromise::defer(*strand_);
            promise->then([]() {}, [](const aasdk::error::Error&) {});
            channel_->sendInputEventIndication(indication, std::move(promise));

            auto t_send = PerfStats::Clock::now();

            // Record metrics
            metricDispatch_.record(PerfStats::msElapsed(t_qmlPoint, t_dispatch));
            metricSend_.record(PerfStats::msElapsed(t_dispatch, t_send));
            metricTotal_.record(PerfStats::msElapsed(t_qmlPoint, t_send));
            ++eventsSinceLog_;

            // Log every 5 seconds
            double secSinceLog = PerfStats::msElapsed(lastLogTime_, t_send) / 1000.0;
            if (secSinceLog >= 5.0) {
                double eventsPerSec = eventsSinceLog_ / secSinceLog;
                BOOST_LOG_TRIVIAL(info)
                    << "[Perf] Touch: qml→dispatch=" << std::fixed << std::setprecision(1)
                    << metricDispatch_.avg() << "ms"
                    << " send=" << metricSend_.avg() << "ms"
                    << " total=" << metricTotal_.avg() << "ms"
                    << " (p99≈" << metricTotal_.max << "ms)"
                    << " | " << std::setprecision(1) << eventsPerSec << " events/sec";
                metricDispatch_.reset();
                metricSend_.reset();
                metricTotal_.reset();
                eventsSinceLog_ = 0;
                lastLogTime_ = t_send;
            }
        });
    }

private:
    std::shared_ptr<aasdk::channel::input::InputServiceChannel> channel_;
    boost::asio::io_service::strand* strand_ = nullptr;

    // Performance instrumentation
    PerfStats::Metric metricDispatch_;
    PerfStats::Metric metricSend_;
    PerfStats::Metric metricTotal_;
    PerfStats::TimePoint lastLogTime_ = PerfStats::Clock::now();
    uint64_t eventsSinceLog_ = 0;
};

} // namespace aa
} // namespace oap
```

**Step 2: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`
Expected: Clean compile

**Step 3: Commit**

```bash
git add src/core/aa/TouchHandler.hpp
git commit -m "feat(perf): instrument touch pipeline with timing metrics"
```

---

### Task 4: Deploy and Capture Baseline Measurements

**Files:** None modified — deployment + observation only

**Context:** Push `perf/aa-streaming` to the Pi, build, run, connect the phone, and capture ~30 seconds of `[Perf]` log output. These are the baseline numbers we're optimizing against.

**Step 1: Push branch to GitHub**

```bash
git push -u origin perf/aa-streaming
```

**Step 2: Pull and build on Pi**

```bash
ssh matt@192.168.1.149 'cd /home/matt/openauto-prodigy && git fetch && git checkout perf/aa-streaming && git pull && cd build && cmake --build . -j3'
```

**Step 3: Run and capture logs**

Terminal 1 (launch):
```bash
ssh matt@192.168.1.149 'pkill -f openauto-prodigy; sleep 1; DISPLAY=:0 XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-0 QT_QPA_PLATFORM=wayland nohup /home/matt/openauto-prodigy/build/src/openauto-prodigy </dev/null >/tmp/oap.log 2>&1 &'
```

Terminal 2 (watch logs):
```bash
ssh matt@192.168.1.149 'tail -f /tmp/oap.log | grep "\[Perf\]"'
```

Connect phone, use AA for 30+ seconds. Record the `[Perf] Video:` and `[Perf] Touch:` lines.

**Step 4: Record baseline in design doc**

Update `docs/plans/2026-02-17-aa-performance-design.md` Phase C table with measured values. Commit:

```bash
git add docs/plans/2026-02-17-aa-performance-design.md
git commit -m "docs: record baseline performance measurements"
```

---

### Task 5: Video Frame Copy Elimination — Cached Format + Stride-Aware Bulk Copy

**Files:**
- Modify: `src/core/aa/VideoDecoder.hpp`
- Modify: `src/core/aa/VideoDecoder.cpp`

**Context:** Currently `QVideoFrameFormat` is constructed and `QVideoFrame` is heap-allocated on every frame (30-60x/sec). We cache the format, pre-allocate two frames (double-buffer), and do bulk `memcpy` per plane when strides match.

**Step 1: Add cached format and double-buffered frames to VideoDecoder.hpp**

After the `PerfStats` member declarations, add:

```cpp
    // Cached video frame format + double-buffered frames
    std::optional<QVideoFrameFormat> cachedFormat_;
    QVideoFrame frameBuffers_[2];
    int currentBuffer_ = 0;
```

Add `#include <optional>` at the top of the header.

**Step 2: Replace the YUV copy and frame construction in VideoDecoder.cpp**

In the `decodeFrame` method, replace the block from `QVideoFrameFormat format(...)` through `videoFrame.unmap()` (the frame construction + 3-plane copy) with:

```cpp
                // Cache format on first frame or resolution change
                if (!cachedFormat_ ||
                    cachedFormat_->frameWidth() != frame_->width ||
                    cachedFormat_->frameHeight() != frame_->height) {
                    cachedFormat_ = QVideoFrameFormat(
                        QSize(frame_->width, frame_->height),
                        QVideoFrameFormat::Format_YUV420P);
                    // Invalidate pre-allocated frames on resolution change
                    frameBuffers_[0] = QVideoFrame();
                    frameBuffers_[1] = QVideoFrame();
                }

                // Double-buffered: alternate between two pre-allocated frames
                int bufIdx = currentBuffer_;
                currentBuffer_ = 1 - currentBuffer_;

                if (!frameBuffers_[bufIdx].isValid()) {
                    frameBuffers_[bufIdx] = QVideoFrame(*cachedFormat_);
                }

                QVideoFrame& videoFrame = frameBuffers_[bufIdx];
                if (videoFrame.map(QVideoFrame::WriteOnly)) {
                    const int h = frame_->height;
                    const int chromaH = h / 2;

                    // Y plane — bulk copy if strides match, line-by-line otherwise
                    const int yDstStride = videoFrame.bytesPerLine(0);
                    const int ySrcStride = frame_->linesize[0];
                    if (yDstStride == ySrcStride) {
                        std::memcpy(videoFrame.bits(0), frame_->data[0], ySrcStride * h);
                    } else {
                        const int yRowBytes = std::min(yDstStride, ySrcStride);
                        for (int y = 0; y < h; ++y)
                            std::memcpy(videoFrame.bits(0) + y * yDstStride,
                                        frame_->data[0] + y * ySrcStride, yRowBytes);
                    }

                    // U plane
                    const int uDstStride = videoFrame.bytesPerLine(1);
                    const int uSrcStride = frame_->linesize[1];
                    if (uDstStride == uSrcStride) {
                        std::memcpy(videoFrame.bits(1), frame_->data[1], uSrcStride * chromaH);
                    } else {
                        const int uRowBytes = std::min(uDstStride, uSrcStride);
                        for (int y = 0; y < chromaH; ++y)
                            std::memcpy(videoFrame.bits(1) + y * uDstStride,
                                        frame_->data[1] + y * uSrcStride, uRowBytes);
                    }

                    // V plane
                    const int vDstStride = videoFrame.bytesPerLine(2);
                    const int vSrcStride = frame_->linesize[2];
                    if (vDstStride == vSrcStride) {
                        std::memcpy(videoFrame.bits(2), frame_->data[2], vSrcStride * chromaH);
                    } else {
                        const int vRowBytes = std::min(vDstStride, vSrcStride);
                        for (int y = 0; y < chromaH; ++y)
                            std::memcpy(videoFrame.bits(2) + y * vDstStride,
                                        frame_->data[2] + y * vSrcStride, vRowBytes);
                    }

                    videoFrame.unmap();
```

And change the `videoSink_->setVideoFrame(videoFrame)` line to remain as-is (it's already a reference).

**Step 3: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`
Expected: Clean compile

**Step 4: Commit**

```bash
git add src/core/aa/VideoDecoder.hpp src/core/aa/VideoDecoder.cpp
git commit -m "perf: cache QVideoFrameFormat, double-buffer frames, stride-aware bulk copy"
```

---

### Task 6: Decode Off Main Thread

**Files:**
- Modify: `src/core/aa/VideoDecoder.hpp`
- Modify: `src/core/aa/VideoDecoder.cpp`
- Modify: `src/core/aa/VideoService.cpp`

**Context:** Currently, `decodeFrame()` runs on the Qt main thread (via `Qt::QueuedConnection`). At 30fps this blocks the main thread ~150ms/sec. Move decode to a dedicated `QThread`. Only `setVideoFrame()` crosses back to main thread.

**Step 1: Add worker thread infrastructure to VideoDecoder.hpp**

Add includes:
```cpp
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <queue>
```

Change the class to own a worker thread. Add these members after the existing private members:

```cpp
    // Decode worker thread
    class DecodeWorker : public QThread {
    public:
        explicit DecodeWorker(VideoDecoder* decoder) : decoder_(decoder) {}
        void run() override;
        void enqueue(QByteArray data, qint64 enqueueTimeNs);
        void requestStop();

    private:
        VideoDecoder* decoder_;
        QMutex mutex_;
        QWaitCondition condition_;
        struct WorkItem { QByteArray data; qint64 enqueueTimeNs; };
        std::queue<WorkItem> queue_;
        bool stopRequested_ = false;
    };

    DecodeWorker* worker_ = nullptr;
    void processFrame(const QByteArray& h264Data, qint64 enqueueTimeNs);
```

Change the public slot:
```cpp
public slots:
    void decodeFrame(QByteArray h264Data, qint64 enqueueTimeNs = 0);
```
This now just enqueues to the worker.

**Step 2: Implement the worker thread in VideoDecoder.cpp**

Add the worker implementation at the bottom of the file (before the closing namespace braces):

```cpp
void VideoDecoder::DecodeWorker::enqueue(QByteArray data, qint64 enqueueTimeNs)
{
    QMutexLocker locker(&mutex_);
    queue_.push({std::move(data), enqueueTimeNs});
    condition_.wakeOne();
}

void VideoDecoder::DecodeWorker::requestStop()
{
    QMutexLocker locker(&mutex_);
    stopRequested_ = true;
    condition_.wakeOne();
}

void VideoDecoder::DecodeWorker::run()
{
    while (true) {
        WorkItem item;
        {
            QMutexLocker locker(&mutex_);
            while (queue_.empty() && !stopRequested_)
                condition_.wait(&mutex_);
            if (stopRequested_ && queue_.empty())
                return;
            item = std::move(queue_.front());
            queue_.pop();
        }
        decoder_->processFrame(item.data, item.enqueueTimeNs);
    }
}
```

Change `decodeFrame` to just enqueue:
```cpp
void VideoDecoder::decodeFrame(QByteArray h264Data, qint64 enqueueTimeNs)
{
    if (worker_)
        worker_->enqueue(std::move(h264Data), enqueueTimeNs);
}
```

Rename the existing decode logic (the big method body) to `processFrame`. The body stays the same except: replace `videoSink_->setVideoFrame(videoFrame)` with:

```cpp
                    QVideoFrame frameCopy = videoFrame;  // shallow copy (refcounted)
                    QMetaObject::invokeMethod(videoSink_, [this, frameCopy]() {
                        videoSink_->setVideoFrame(frameCopy);
                    }, Qt::QueuedConnection);
```

Start the worker in the constructor (after FFmpeg init succeeds):
```cpp
    worker_ = new DecodeWorker(this);
    worker_->start();
    BOOST_LOG_TRIVIAL(info) << "[VideoDecoder] Decode worker thread started";
```

Stop the worker in `cleanup()`:
```cpp
    if (worker_) {
        worker_->requestStop();
        worker_->wait();
        delete worker_;
        worker_ = nullptr;
    }
```

**Step 3: Update VideoService connection**

In `VideoService.cpp`, the signal-slot connection (line 32-34) currently uses `Qt::QueuedConnection` to marshal to main thread. Change it to `Qt::DirectConnection` — the signal emits on ASIO thread, and now `decodeFrame` just enqueues (thread-safe), so no need for Qt thread crossing:

```cpp
    if (decoder_) {
        connect(this, &VideoService::videoFrameData,
                decoder_, &VideoDecoder::decodeFrame,
                Qt::DirectConnection);
    }
```

Wait — actually `Qt::DirectConnection` would call `decodeFrame` on the ASIO thread. Since `decodeFrame` now just does `worker_->enqueue()` which is mutex-protected, this is safe and eliminates one unnecessary thread hop. The data goes: ASIO thread → worker thread → main thread (for setVideoFrame only).

**Step 4: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`
Expected: Clean compile

**Step 5: Commit**

```bash
git add src/core/aa/VideoDecoder.hpp src/core/aa/VideoDecoder.cpp src/core/aa/VideoService.cpp
git commit -m "perf: move video decode to dedicated worker thread"
```

---

### Task 7: Touch Event Batching

**Files:**
- Modify: `src/core/aa/TouchHandler.hpp`
- Modify: `qml/applications/android_auto/AndroidAutoMenu.qml`

**Context:** Currently each finger sends its own `InputEventIndication` + `strand_.dispatch()` + `SendPromise`. AA protocol supports multiple `touch_location` entries per message. Batch all points from one QML callback into one message.

**Step 1: Add batch method to TouchHandler.hpp**

Add a new `Q_INVOKABLE` method after `sendMultiTouchEvent`:

```cpp
    // Batch all touch points from one event into a single protobuf message
    Q_INVOKABLE void sendBatchTouchEvent(QVariantList points, int action) {
        if (!channel_ || !strand_ || points.isEmpty()) return;

        auto t_qml = PerfStats::Clock::now().time_since_epoch().count();

        strand_->dispatch([this, points = std::move(points), action, t_qml]() {
            auto t_dispatch = PerfStats::Clock::now();
            auto t_qmlPoint = PerfStats::TimePoint(
                std::chrono::nanoseconds(t_qml));

            aasdk::proto::messages::InputEventIndication indication;
            indication.set_timestamp(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            indication.set_disp_channel(0);

            auto* touchEvent = indication.mutable_touch_event();
            touchEvent->set_touch_action(
                static_cast<aasdk::proto::enums::TouchAction::Enum>(action));

            // First pointer is the action_index for POINTER_DOWN/POINTER_UP
            if (!points.isEmpty()) {
                auto firstPt = points[0].toMap();
                touchEvent->set_action_index(firstPt["pointerId"].toInt());
            }

            for (const auto& pt : points) {
                auto map = pt.toMap();
                auto* location = touchEvent->add_touch_location();
                location->set_x(map["x"].toInt());
                location->set_y(map["y"].toInt());
                location->set_pointer_id(map["pointerId"].toInt());
            }

            auto promise = aasdk::channel::SendPromise::defer(*strand_);
            promise->then([]() {}, [](const aasdk::error::Error&) {});
            channel_->sendInputEventIndication(indication, std::move(promise));

            auto t_send = PerfStats::Clock::now();

            metricDispatch_.record(PerfStats::msElapsed(t_qmlPoint, t_dispatch));
            metricSend_.record(PerfStats::msElapsed(t_dispatch, t_send));
            metricTotal_.record(PerfStats::msElapsed(t_qmlPoint, t_send));
            ++eventsSinceLog_;

            double secSinceLog = PerfStats::msElapsed(lastLogTime_, t_send) / 1000.0;
            if (secSinceLog >= 5.0) {
                double eventsPerSec = eventsSinceLog_ / secSinceLog;
                BOOST_LOG_TRIVIAL(info)
                    << "[Perf] Touch: qml→dispatch=" << std::fixed << std::setprecision(1)
                    << metricDispatch_.avg() << "ms"
                    << " send=" << metricSend_.avg() << "ms"
                    << " total=" << metricTotal_.avg() << "ms"
                    << " (p99≈" << metricTotal_.max << "ms)"
                    << " | " << std::setprecision(1) << eventsPerSec << " events/sec";
                metricDispatch_.reset();
                metricSend_.reset();
                metricTotal_.reset();
                eventsSinceLog_ = 0;
                lastLogTime_ = t_send;
            }
        });
    }
```

Add the include for QVariantList:
```cpp
#include <QVariant>
```

**Step 2: Update QML to use batch API**

Replace the three handlers in `AndroidAutoMenu.qml`:

```qml
        onPressed: function(touchPoints) {
            var points = [];
            for (var i = 0; i < touchPoints.length; i++) {
                var tp = touchPoints[i];
                points.push({
                    "x": Math.round(tp.x / width * 1024),
                    "y": Math.round(tp.y / height * 600),
                    "pointerId": tp.pointId
                });
                touchPointModel.append({ ptId: tp.pointId, ptX: tp.x, ptY: tp.y });
            }
            // First finger = PRESS (0), additional = POINTER_DOWN (5)
            TouchHandler.sendBatchTouchEvent(points, points.length === 1 ? 0 : 5);
        }
        onReleased: function(touchPoints) {
            var points = [];
            for (var i = 0; i < touchPoints.length; i++) {
                var tp = touchPoints[i];
                points.push({
                    "x": Math.round(tp.x / width * 1024),
                    "y": Math.round(tp.y / height * 600),
                    "pointerId": tp.pointId
                });
                for (var j = touchPointModel.count - 1; j >= 0; j--) {
                    if (touchPointModel.get(j).ptId === tp.pointId)
                        touchPointModel.remove(j);
                }
            }
            // Last finger = RELEASE (1), others = POINTER_UP (6)
            TouchHandler.sendBatchTouchEvent(points, points.length === 1 ? 1 : 6);
        }
        onUpdated: function(touchPoints) {
            var points = [];
            for (var i = 0; i < touchPoints.length; i++) {
                var tp = touchPoints[i];
                points.push({
                    "x": Math.round(tp.x / width * 1024),
                    "y": Math.round(tp.y / height * 600),
                    "pointerId": tp.pointId
                });
                for (var j = 0; j < touchPointModel.count; j++) {
                    if (touchPointModel.get(j).ptId === tp.pointId) {
                        touchPointModel.set(j, { ptId: tp.pointId, ptX: tp.x, ptY: tp.y });
                        break;
                    }
                }
            }
            TouchHandler.sendBatchTouchEvent(points, 2);  // DRAG
        }
```

**Step 3: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`
Expected: Clean compile

**Step 4: Commit**

```bash
git add src/core/aa/TouchHandler.hpp qml/applications/android_auto/AndroidAutoMenu.qml
git commit -m "perf: batch multi-touch events into single protobuf message"
```

---

### Task 8: Configurable Video FPS (30/60)

**Files:**
- Modify: `src/core/Configuration.hpp`
- Modify: `src/core/Configuration.cpp`
- Modify: `src/core/aa/VideoService.cpp`
- Modify: `tests/test_configuration.cpp`

**Context:** AA protocol supports 60fps (`VideoFPS::_60 = 1`). Currently hardcoded to 30fps. Add INI config `[Video] fps=60` and read it in `fillFeatures`.

**Step 1: Add videoFps to Configuration.hpp**

After `uint16_t m_tcpPort = 5288;` (line 169), add:

```cpp
    // Video
    int m_videoFps = 60;
```

In the public section, after the Wireless getters/setters, add:

```cpp
    // --- Video ---
    int videoFps() const { return m_videoFps; }
    void setVideoFps(int v) { m_videoFps = (v == 30) ? 30 : 60; }
```

**Step 2: Add load/save for videoFps in Configuration.cpp**

In `load()`, after the `[Wireless]` section (after line 144 `settings.endGroup()`), add:

```cpp
    // [Video]
    settings.beginGroup(QStringLiteral("Video"));
    m_videoFps = settings.value(QStringLiteral("fps"), m_videoFps).toInt();
    if (m_videoFps != 30 && m_videoFps != 60) m_videoFps = 60;
    settings.endGroup();
```

In `save()`, after the `[Wireless]` section (after line 192 `settings.endGroup()`), add:

```cpp
    // [Video]
    settings.beginGroup(QStringLiteral("Video"));
    settings.setValue(QStringLiteral("fps"), m_videoFps);
    settings.endGroup();
```

**Step 3: Use config in VideoService::fillFeatures**

In `VideoService.cpp`, `fillFeatures` method, change line 62:

```cpp
    videoConfig->set_video_fps(aasdk::proto::enums::VideoFPS::_30);
```
to:
```cpp
    videoConfig->set_video_fps(
        config_->videoFps() == 60
            ? aasdk::proto::enums::VideoFPS::_60
            : aasdk::proto::enums::VideoFPS::_30);
```

**Step 4: Add test for videoFps config**

Read the existing test file to understand the pattern:

In `tests/test_configuration.cpp`, add a test case:

```cpp
void TestConfiguration::testVideoFps()
{
    // Default should be 60
    oap::Configuration config;
    QCOMPARE(config.videoFps(), 60);

    // Set to 30
    config.setVideoFps(30);
    QCOMPARE(config.videoFps(), 30);

    // Invalid values default to 60
    config.setVideoFps(45);
    QCOMPARE(config.videoFps(), 60);
}
```

Add `void testVideoFps();` to the test class declaration and add the slot.

**Step 5: Build and run tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass including new `testVideoFps`

**Step 6: Commit**

```bash
git add src/core/Configuration.hpp src/core/Configuration.cpp src/core/aa/VideoService.cpp tests/test_configuration.cpp
git commit -m "feat: configurable video FPS (30/60, default 60)"
```

---

### Task 9: Deploy Optimized Build and Measure

**Files:** None modified — deployment + comparison

**Context:** Deploy the full optimized branch to the Pi. Capture `[Perf]` logs and compare against Task 4 baseline. This is the Phase C review from the design doc.

**Step 1: Push and deploy**

```bash
git push origin perf/aa-streaming
ssh matt@192.168.1.149 'cd /home/matt/openauto-prodigy && git pull && cd build && cmake --build . -j3'
```

**Step 2: Run and capture optimized measurements**

Same as Task 4 Step 3. Record the `[Perf]` lines.

**Step 3: Compare against targets**

| Metric | Target | Baseline | Optimized |
|--------|--------|----------|-----------|
| Video frame latency (p99) | < 10ms | (from Task 4) | (measured) |
| Touch latency (p99) | < 5ms | (from Task 4) | (measured) |
| Main thread blocked by decode | < 20ms/sec | ~150ms/sec | (measured) |
| Dropped frames | 0 | (from Task 4) | (measured) |
| Video FPS | 60 | 30 | (measured) |

**Step 4: Update design doc with results**

```bash
git add docs/plans/2026-02-17-aa-performance-design.md
git commit -m "docs: record optimized performance measurements and comparison"
```

**Step 5: Decision point**

- **Hit targets:** Merge `perf/aa-streaming` → `main`. Done.
- **Miss targets:** The instrumentation shows exactly where time is spent. Consider advanced techniques per Phase C of the design doc.

---

### Task 10: Merge to Main

**Files:** None — git operations only

**Context:** Only if Task 9 measurements meet targets.

**Step 1: Merge**

```bash
git checkout main
git merge perf/aa-streaming
git push origin main
```

Or use PR workflow per `pr-workflow` skill if preferred.
