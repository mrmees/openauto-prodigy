# AV Pipeline Optimization Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Reduce CPU usage, latency, and memory allocations across the AA audio/video pipeline to support 1080p@30 on Pi 4 and broaden viable hardware.

**Architecture:** Five optimization layers, each independently shippable. Audio latency first (highest user impact), then transport copy reduction, codec detection, video frame pool, FFmpeg hw decoder selection, and finally zero-copy QAbstractVideoBuffer for Qt 6.8+.

**Tech Stack:** C++17, Qt 6.4+/6.8, FFmpeg (libavcodec/libavformat), PipeWire (libpipewire), spa_ringbuffer, CMake

**Design doc:** `docs/plans/2026-02-25-av-pipeline-optimization-design.md`

---

## Task 1: Audio — Per-Stream Ring Buffer Sizing

**Files:**
- Modify: `src/core/services/AudioService.cpp:156-160`
- Modify: `src/core/services/AudioService.hpp:18-38`
- Modify: `src/core/YamlConfig.hpp:48-50`
- Modify: `src/core/YamlConfig.cpp:35-47`
- Test: `tests/test_audio_service.cpp`

**Context:** Ring buffer is hardcoded at 100ms (`sampleRate * channels * 2 * 0.1` on line 157 of AudioService.cpp). We need per-stream sizing configurable via YAML.

**Step 1: Add YAML config for per-stream buffer sizing**

In `src/core/YamlConfig.cpp`, add defaults after the existing audio defaults:

```cpp
root_["audio"]["buffer_ms"]["media"] = 50;
root_["audio"]["buffer_ms"]["speech"] = 35;
root_["audio"]["buffer_ms"]["system"] = 35;
```

In `src/core/YamlConfig.hpp`, add accessor:

```cpp
int audioBufferMs(const QString& streamType) const;
```

Implementation in `.cpp`:
```cpp
int YamlConfig::audioBufferMs(const QString& streamType) const {
    std::string key = streamType.toStdString();
    if (root_["audio"]["buffer_ms"][key])
        return root_["audio"]["buffer_ms"][key].as<int>();
    return 50; // safe default
}
```

**Step 2: Pass buffer size to createStream()**

In `AudioService.hpp`, add `bufferMs` parameter to `AudioStreamHandle`:
```cpp
int bufferMs = 50;  // ring buffer size in milliseconds
```

In `AudioService.cpp:156-160`, replace the hardcoded 0.1:
```cpp
float bufferSec = handle->bufferMs / 1000.0f;
uint32_t rbSize = static_cast<uint32_t>(sampleRate * channels * 2 * bufferSec);
uint32_t pow2 = 1;
while (pow2 < rbSize) pow2 <<= 1;
handle->ringBuffer = std::make_unique<AudioRingBuffer>(pow2);
```

**Step 3: Wire config to stream creation in AndroidAutoOrchestrator**

Where media/speech/system streams are created, pass the YAML buffer_ms value:
```cpp
handle->bufferMs = yamlConfig_->audioBufferMs("media");  // or "speech", "system"
```

**Step 4: Build and test**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass, no build errors.

**Step 5: Commit**

```bash
git add src/core/services/AudioService.cpp src/core/services/AudioService.hpp \
        src/core/YamlConfig.hpp src/core/YamlConfig.cpp
git commit -m "feat: per-stream audio ring buffer sizing (media=50ms, speech/system=35ms)"
```

---

## Task 2: Audio — Silence Insertion at Water-Mark

**Files:**
- Modify: `src/core/services/AudioService.cpp:117-136`
- Test: `tests/test_audio_service.cpp`

**Context:** The PipeWire RT callback (`onPlaybackProcess`) currently returns short reads if the ring buffer is partially empty. This causes audio discontinuities. We need a minimum water-mark: if less than one period of data is available, fill remainder with silence.

**Step 1: Add silence insertion to RT callback**

In `AudioService.cpp`, modify `onPlaybackProcess()` (lines 117-136):

```cpp
static void onPlaybackProcess(void* userdata)
{
    auto* handle = static_cast<AudioStreamHandle*>(userdata);
    struct pw_buffer* buf = pw_stream_dequeue_buffer(handle->stream);
    if (!buf) return;

    auto& d = buf->buffer->datas[0];
    uint8_t* dst = static_cast<uint8_t*>(d.data);
    uint32_t maxSize = d.maxsize;

    uint32_t bytesRead = handle->ringBuffer->read(dst, maxSize);

    // Silence-fill remainder if we got a short read (prevents audio glitches)
    if (bytesRead < maxSize) {
        std::memset(dst + bytesRead, 0, maxSize - bytesRead);
    }

    d.chunk->offset = 0;
    d.chunk->stride = handle->bytesPerFrame;
    d.chunk->size = maxSize;  // always report full period to PipeWire

    pw_stream_queue_buffer(handle->stream, buf);
}
```

Note: We now always report `maxSize` to PipeWire but silence-fill any gap. This prevents PipeWire from seeing partial periods.

**Step 2: Build and test**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`

**Step 3: Commit**

```bash
git add src/core/services/AudioService.cpp
git commit -m "fix: silence-fill short reads in PipeWire RT callback"
```

---

## Task 3: Audio — Adaptive Buffer Growth

**Files:**
- Modify: `src/core/services/AudioService.hpp:18-38`
- Modify: `src/core/services/AudioService.cpp:117-136`
- Modify: `src/core/YamlConfig.cpp` (add `audio.adaptive` default)
- Create: `src/core/services/AudioRingBuffer.hpp` (if not already adaptive-aware)
- Test: `tests/test_audio_service.cpp`

**Context:** At 35-50ms buffer sizes, underruns are possible on loaded systems. Monitor PipeWire xrun count and grow buffer if underruns are detected.

**Step 1: Add underrun tracking to AudioStreamHandle**

In `AudioService.hpp`, add to `AudioStreamHandle`:
```cpp
std::atomic<uint32_t> underrunCount{0};
std::chrono::steady_clock::time_point lastGrowTime{};
int currentBufferMs = 50;
int maxBufferMs = 100;
```

**Step 2: Detect underruns in RT callback**

In `onPlaybackProcess()`, after the ring buffer read:
```cpp
if (bytesRead == 0) {
    handle->underrunCount.fetch_add(1, std::memory_order_relaxed);
}
```

**Step 3: Add periodic underrun check on main thread**

In `AudioService`, add a QTimer (1-second interval) that checks each stream's underrun count. If underruns > 2 since last check and `adaptive` is enabled:
```cpp
void AudioService::checkUnderruns()
{
    for (auto* handle : streams_) {
        uint32_t xruns = handle->underrunCount.exchange(0, std::memory_order_relaxed);
        if (xruns >= 2 && handle->currentBufferMs < handle->maxBufferMs) {
            handle->currentBufferMs = std::min(handle->currentBufferMs + 10, handle->maxBufferMs);
            handle->ringBuffer->resize(/* new size from currentBufferMs */);
            qInfo() << "[Audio] Buffer grown to" << handle->currentBufferMs
                    << "ms for stream" << handle->name << "(" << xruns << "xruns)";
        }
    }
}
```

**Step 4: Add YAML config**

In `YamlConfig.cpp` defaults:
```cpp
root_["audio"]["adaptive"] = true;
```

**Step 5: Build and test**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`

**Step 6: Commit**

```bash
git add src/core/services/AudioService.hpp src/core/services/AudioService.cpp \
        src/core/YamlConfig.cpp
git commit -m "feat: adaptive audio buffer growth on underrun detection"
```

---

## Task 4: Transport — FrameParser Circular Buffer

**Files:**
- Create: `libs/open-androidauto/include/oaa/Messenger/CircularBuffer.hpp`
- Modify: `libs/open-androidauto/include/oaa/Messenger/FrameParser.hpp:32`
- Modify: `libs/open-androidauto/src/Messenger/FrameParser.cpp:14-56`
- Modify: `libs/open-androidauto/CMakeLists.txt`
- Test: `tests/test_circular_buffer.cpp`

**Context:** FrameParser uses `QByteArray` with `append()` + `remove(0, N)`. The `remove` does memmove of the entire remaining buffer every frame. Replace with a circular buffer.

**Step 1: Write the failing test**

Create `tests/test_circular_buffer.cpp`:
```cpp
#include <QTest>
#include "oaa/Messenger/CircularBuffer.hpp"

class TestCircularBuffer : public QObject {
    Q_OBJECT
private slots:
    void testBasicWriteRead();
    void testWrapAround();
    void testGrow();
    void testPeek();
    void testConsume();
};

void TestCircularBuffer::testBasicWriteRead()
{
    oaa::CircularBuffer buf(64);
    QByteArray data("hello world");
    buf.append(data.constData(), data.size());
    QCOMPARE(buf.available(), 11);

    QByteArray out = buf.peek(11);
    QCOMPARE(out, data);

    buf.consume(11);
    QCOMPARE(buf.available(), 0);
}

void TestCircularBuffer::testWrapAround()
{
    oaa::CircularBuffer buf(16);
    // Fill most of buffer
    QByteArray fill(12, 'A');
    buf.append(fill.constData(), fill.size());
    buf.consume(12);  // read cursor at 12

    // Write data that wraps around
    QByteArray wrap("HELLO WORLD!");  // 12 bytes, wraps from pos 12 back to start
    buf.append(wrap.constData(), wrap.size());

    QByteArray out = buf.peek(12);
    QCOMPARE(out, wrap);
}

void TestCircularBuffer::testGrow()
{
    oaa::CircularBuffer buf(8);
    QByteArray data(16, 'X');  // larger than initial capacity
    buf.append(data.constData(), data.size());  // should auto-grow
    QCOMPARE(buf.available(), 16);
    QCOMPARE(buf.peek(16), data);
}

void TestCircularBuffer::testPeek()
{
    oaa::CircularBuffer buf(64);
    QByteArray data("test data");
    buf.append(data.constData(), data.size());

    // Peek doesn't consume
    QByteArray p1 = buf.peek(9);
    QByteArray p2 = buf.peek(9);
    QCOMPARE(p1, p2);
    QCOMPARE(buf.available(), 9);
}

void TestCircularBuffer::testConsume()
{
    oaa::CircularBuffer buf(64);
    QByteArray data("abcdefghij");
    buf.append(data.constData(), data.size());

    buf.consume(3);
    QCOMPARE(buf.available(), 7);
    QCOMPARE(buf.peek(7), QByteArray("defghij"));
}

QTEST_MAIN(TestCircularBuffer)
#include "test_circular_buffer.moc"
```

Add to `tests/CMakeLists.txt`:
```cmake
add_executable(test_circular_buffer test_circular_buffer.cpp)
target_link_libraries(test_circular_buffer PRIVATE Qt6::Test open-androidauto)
add_test(NAME test_circular_buffer COMMAND test_circular_buffer)
```

**Step 2: Run test to verify it fails**

Run: `cd build && cmake .. && cmake --build . -j$(nproc) --target test_circular_buffer`
Expected: FAIL — CircularBuffer.hpp doesn't exist yet.

**Step 3: Implement CircularBuffer**

Create `libs/open-androidauto/include/oaa/Messenger/CircularBuffer.hpp`:
```cpp
#pragma once
#include <QByteArray>
#include <vector>
#include <cstring>
#include <algorithm>

namespace oaa {

class CircularBuffer {
public:
    explicit CircularBuffer(size_t initialCapacity = 65536)
        : data_(initialCapacity), capacity_(initialCapacity) {}

    void append(const char* src, size_t len)
    {
        if (available() + len > capacity_)
            grow(available() + len);

        size_t firstChunk = std::min(len, capacity_ - writePos_);
        std::memcpy(data_.data() + writePos_, src, firstChunk);
        if (len > firstChunk)
            std::memcpy(data_.data(), src + firstChunk, len - firstChunk);
        writePos_ = (writePos_ + len) % capacity_;
        size_ += len;
    }

    size_t available() const { return size_; }

    /// Peek at the next `len` bytes without consuming them.
    /// Returns a QByteArray (may require linearization if data wraps).
    QByteArray peek(size_t len) const
    {
        len = std::min(len, size_);
        if (len == 0) return {};

        size_t firstChunk = std::min(len, capacity_ - readPos_);
        if (firstChunk == len) {
            return QByteArray(data_.data() + readPos_, len);
        }
        // Wrap case: must linearize
        QByteArray result(len, Qt::Uninitialized);
        std::memcpy(result.data(), data_.data() + readPos_, firstChunk);
        std::memcpy(result.data() + firstChunk, data_.data(), len - firstChunk);
        return result;
    }

    /// Access contiguous data at readPos. Returns pointer and sets `contigLen`
    /// to how many bytes are available contiguously (without wrap).
    const char* readPtr(size_t& contigLen) const
    {
        contigLen = std::min(size_, capacity_ - readPos_);
        return data_.data() + readPos_;
    }

    void consume(size_t len)
    {
        len = std::min(len, size_);
        readPos_ = (readPos_ + len) % capacity_;
        size_ -= len;
    }

private:
    void grow(size_t minCapacity)
    {
        size_t newCap = capacity_;
        while (newCap < minCapacity) newCap *= 2;

        std::vector<char> newData(newCap);
        // Linearize existing data into new buffer
        if (size_ > 0) {
            size_t firstChunk = std::min(size_, capacity_ - readPos_);
            std::memcpy(newData.data(), data_.data() + readPos_, firstChunk);
            if (size_ > firstChunk)
                std::memcpy(newData.data() + firstChunk, data_.data(), size_ - firstChunk);
        }
        data_ = std::move(newData);
        capacity_ = newCap;
        readPos_ = 0;
        writePos_ = size_;
    }

    std::vector<char> data_;
    size_t capacity_;
    size_t readPos_ = 0;
    size_t writePos_ = 0;
    size_t size_ = 0;
};

} // namespace oaa
```

**Step 4: Run test to verify it passes**

Run: `cd build && cmake .. && cmake --build . -j$(nproc) --target test_circular_buffer && ./tests/test_circular_buffer`
Expected: PASS

**Step 5: Integrate into FrameParser**

In `FrameParser.hpp`, replace `QByteArray m_buffer` with:
```cpp
#include "oaa/Messenger/CircularBuffer.hpp"
// ...
oaa::CircularBuffer m_buffer{65536};
```

In `FrameParser.cpp`, update `process()`:
- Replace `m_buffer.append(data)` with `m_buffer.append(data.constData(), data.size())`
- Replace `m_buffer.remove(0, N)` with `m_buffer.consume(N)`
- Replace `m_buffer.size()` with `m_buffer.available()`
- Replace `m_buffer.left(N)` / `m_buffer.mid(off, len)` with `m_buffer.peek(N)` or appropriate read operations

**Step 6: Build and run full test suite**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass.

**Step 7: Commit**

```bash
git add libs/open-androidauto/include/oaa/Messenger/CircularBuffer.hpp \
        libs/open-androidauto/include/oaa/Messenger/FrameParser.hpp \
        libs/open-androidauto/src/Messenger/FrameParser.cpp \
        tests/test_circular_buffer.cpp tests/CMakeLists.txt
git commit -m "perf: replace FrameParser QByteArray with circular buffer"
```

---

## Task 5: Transport — Messenger Zero-Copy Message ID Extraction

**Files:**
- Modify: `libs/open-androidauto/include/oaa/Channel/IChannelHandler.hpp:18`
- Modify: `libs/open-androidauto/src/Messenger/Messenger.cpp`
- Modify: All channel handler `.cpp` files (7 handlers in `libs/open-androidauto/src/HU/Handlers/`)
- Test: `tests/test_oaa_integration.cpp`

**Context:** `Messenger` currently does `payload.mid(2)` to strip the 2-byte message ID, allocating a new QByteArray for every message. Instead, pass the full payload + offset to handlers.

**Step 1: Update IChannelHandler interface**

In `IChannelHandler.hpp`, change `onMessage` signature:
```cpp
virtual void onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset = 0) = 0;
```

The default `dataOffset = 0` preserves backward compatibility for any external consumers.

**Step 2: Update Messenger to pass offset instead of copying**

In `Messenger.cpp`, where it currently does:
```cpp
QByteArray msgPayload = payload.mid(2);
handler->onMessage(messageId, msgPayload);
```

Change to:
```cpp
handler->onMessage(messageId, payload, 2);
```

**Step 3: Update all channel handlers**

In each handler's `onMessage()`, change protobuf parsing from:
```cpp
msg.ParseFromArray(payload.constData(), payload.size());
```
to:
```cpp
msg.ParseFromArray(payload.constData() + dataOffset, payload.size() - dataOffset);
```

Handlers to update:
- `VideoChannelHandler.cpp`
- `AudioChannelHandler.cpp`
- `InputChannelHandler.cpp`
- `SensorChannelHandler.cpp`
- `BluetoothChannelHandler.cpp`
- `WiFiChannelHandler.cpp`
- `AVInputChannelHandler.cpp`
- `NavigationChannelHandler.cpp`
- `MediaStatusChannelHandler.cpp`
- `PhoneStatusChannelHandler.cpp`
- `StubChannelHandler.cpp` (if it has onMessage)

**Step 4: Build and run full test suite**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass (especially test_oaa_integration and all handler tests).

**Step 5: Commit**

```bash
git add libs/open-androidauto/include/oaa/Channel/IChannelHandler.hpp \
        libs/open-androidauto/src/Messenger/Messenger.cpp \
        libs/open-androidauto/src/HU/Handlers/*.cpp
git commit -m "perf: zero-copy message ID extraction in Messenger"
```

---

## Task 6: Codec Capability Detection

**Files:**
- Create: `src/core/aa/CodecCapability.hpp`
- Create: `src/core/aa/CodecCapability.cpp`
- Modify: `src/core/YamlConfig.hpp`
- Modify: `src/core/YamlConfig.cpp`
- Modify: `src/CMakeLists.txt`
- Test: `tests/test_codec_capability.cpp`

**Context:** Probe FFmpeg at startup for available decoders per codec. Store results in YAML capabilities section. ServiceDiscoveryBuilder reads these to build the codec list.

**Step 1: Write the failing test**

Create `tests/test_codec_capability.cpp`:
```cpp
#include <QTest>
#include "core/aa/CodecCapability.hpp"

class TestCodecCapability : public QObject {
    Q_OBJECT
private slots:
    void testProbeFindsH264Software();
    void testProbeResultStructure();
    void testCodecListFromCapabilities();
};

void TestCodecCapability::testProbeFindsH264Software()
{
    auto caps = oap::aa::CodecCapability::probe();
    // H.264 software decoder should always be available
    auto it = caps.find("h264");
    QVERIFY(it != caps.end());
    QVERIFY(!it->second.software.empty());
}

void TestCodecCapability::testProbeResultStructure()
{
    auto caps = oap::aa::CodecCapability::probe();
    for (auto& [codec, cap] : caps) {
        // Each entry has a codec name and at least one decoder
        QVERIFY(!codec.empty());
        QVERIFY(!cap.software.empty() || !cap.hardware.empty());
    }
}

void TestCodecCapability::testCodecListFromCapabilities()
{
    auto caps = oap::aa::CodecCapability::probe();
    auto codecs = oap::aa::CodecCapability::availableCodecs(caps);
    // H.264 must always be present
    QVERIFY(std::find(codecs.begin(), codecs.end(), "h264") != codecs.end());
}

QTEST_MAIN(TestCodecCapability)
#include "test_codec_capability.moc"
```

**Step 2: Implement CodecCapability**

Create `src/core/aa/CodecCapability.hpp`:
```cpp
#pragma once
#include <string>
#include <vector>
#include <map>

namespace oap {
namespace aa {

struct DecoderInfo {
    std::string name;       // e.g. "h264_v4l2m2m"
    bool isHardware;
};

struct CodecInfo {
    std::vector<DecoderInfo> hardware;
    std::vector<DecoderInfo> software;
};

class CodecCapability {
public:
    /// Probe FFmpeg for available decoders. Returns map of codec name -> CodecInfo.
    static std::map<std::string, CodecInfo> probe();

    /// Return list of codec names that have at least one available decoder.
    static std::vector<std::string> availableCodecs(
        const std::map<std::string, CodecInfo>& caps);
};

} // namespace aa
} // namespace oap
```

Create `src/core/aa/CodecCapability.cpp`:
```cpp
#include "CodecCapability.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
}
#include <QDebug>

namespace oap {
namespace aa {

struct ProbeEntry {
    const char* codec;       // our name
    const char* decoder;     // FFmpeg decoder name
    bool isHardware;
};

static const ProbeEntry probeList[] = {
    {"h264", "h264_v4l2m2m", true},
    {"h264", "h264_vaapi",   true},
    {"h264", "h264",         false},

    {"h265", "hevc_v4l2m2m", true},
    {"h265", "hevc_vaapi",   true},
    {"h265", "hevc",         false},

    {"vp9",  "vp9_v4l2m2m",  true},
    {"vp9",  "vp9_vaapi",    true},
    {"vp9",  "libvpx-vp9",   false},
    {"vp9",  "vp9",          false},

    {"av1",  "av1_v4l2m2m",  true},
    {"av1",  "av1_vaapi",    true},
    {"av1",  "libdav1d",     false},
    {"av1",  "av1",          false},
};

std::map<std::string, CodecInfo> CodecCapability::probe()
{
    std::map<std::string, CodecInfo> result;
    for (const auto& entry : probeList) {
        const AVCodec* dec = avcodec_find_decoder_by_name(entry.decoder);
        if (dec) {
            DecoderInfo info{entry.decoder, entry.isHardware};
            if (entry.isHardware)
                result[entry.codec].hardware.push_back(info);
            else
                result[entry.codec].software.push_back(info);
            qInfo() << "[CodecCapability] Found:" << entry.decoder
                    << (entry.isHardware ? "(hw)" : "(sw)")
                    << "for" << entry.codec;
        }
    }
    return result;
}

std::vector<std::string> CodecCapability::availableCodecs(
    const std::map<std::string, CodecInfo>& caps)
{
    std::vector<std::string> result;
    for (const auto& [codec, info] : caps) {
        if (!info.hardware.empty() || !info.software.empty())
            result.push_back(codec);
    }
    return result;
}

} // namespace aa
} // namespace oap
```

**Step 3: Add to CMakeLists.txt**

In `src/CMakeLists.txt`, add `core/aa/CodecCapability.cpp` to the source list.

In `tests/CMakeLists.txt`, add the test:
```cmake
add_executable(test_codec_capability test_codec_capability.cpp
    ${CMAKE_SOURCE_DIR}/src/core/aa/CodecCapability.cpp)
target_link_libraries(test_codec_capability PRIVATE Qt6::Test PkgConfig::LIBAV)
add_test(NAME test_codec_capability COMMAND test_codec_capability)
```

**Step 4: Run tests**

Run: `cd build && cmake .. && cmake --build . -j$(nproc) && ctest --output-on-failure`

**Step 5: Commit**

```bash
git add src/core/aa/CodecCapability.hpp src/core/aa/CodecCapability.cpp \
        src/CMakeLists.txt tests/test_codec_capability.cpp tests/CMakeLists.txt
git commit -m "feat: FFmpeg codec capability detection at startup"
```

---

## Task 7: Wire Codec Capabilities to ServiceDiscoveryBuilder

**Files:**
- Modify: `src/core/aa/ServiceDiscoveryBuilder.hpp`
- Modify: `src/core/aa/ServiceDiscoveryBuilder.cpp:138-176`
- Modify: `src/core/YamlConfig.hpp`
- Modify: `src/core/YamlConfig.cpp`

**Context:** ServiceDiscoveryBuilder currently hardcodes H.264 + H.265. It should read enabled codecs from config (which was populated by capability detection).

**Step 1: Add codec config to YamlConfig**

In `YamlConfig.cpp` defaults:
```cpp
root_["video"]["codecs"] = YAML::Node(YAML::NodeType::Sequence);
root_["video"]["codecs"].push_back("h264");
root_["video"]["codecs"].push_back("h265");
```

In `YamlConfig.hpp`:
```cpp
QStringList videoCodecs() const;
```

Implementation:
```cpp
QStringList YamlConfig::videoCodecs() const {
    QStringList result;
    if (root_["video"]["codecs"] && root_["video"]["codecs"].IsSequence()) {
        for (const auto& node : root_["video"]["codecs"])
            result.append(QString::fromStdString(node.as<std::string>()));
    }
    if (result.isEmpty()) result.append("h264");  // always have H.264
    return result;
}
```

**Step 2: Update ServiceDiscoveryBuilder to use config codecs**

In `ServiceDiscoveryBuilder.cpp`, replace the hardcoded codec list (lines 149-153) with a dynamic list built from `yamlConfig_->videoCodecs()`. Map codec name strings to protobuf enum values:

```cpp
static const QMap<QString, Codec> codecMap = {
    {"h264", Codec::MEDIA_CODEC_VIDEO_H264_BP},
    {"h265", Codec::MEDIA_CODEC_VIDEO_H265},
    {"vp9",  Codec::MEDIA_CODEC_VIDEO_VP9},
    {"av1",  Codec::MEDIA_CODEC_VIDEO_AV1},
};

QStringList enabledCodecs = yamlConfig_->videoCodecs();
for (const auto& codecName : enabledCodecs) {
    auto it = codecMap.find(codecName);
    if (it == codecMap.end()) continue;
    // ... add video config with this codec enum
}
```

**Step 3: Build and test**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`

**Step 4: Commit**

```bash
git add src/core/aa/ServiceDiscoveryBuilder.cpp src/core/aa/ServiceDiscoveryBuilder.hpp \
        src/core/YamlConfig.hpp src/core/YamlConfig.cpp
git commit -m "feat: ServiceDiscovery codec list driven by config/capabilities"
```

---

## Task 8: Video Settings UI — Codec & Decoder Selection

**Files:**
- Modify: `qml/applications/settings/VideoSettings.qml`
- Create: `src/ui/CodecCapabilityModel.hpp`
- Create: `src/ui/CodecCapabilityModel.cpp`
- Modify: `src/main.cpp` (register model with QML)
- Modify: `src/CMakeLists.txt`

**Context:** Settings page needs Hardware/Software toggle per codec, with a dropdown of available decoders that repopulates on toggle.

**Step 1: Create QML model for codec capabilities**

Create `src/ui/CodecCapabilityModel.hpp` — a QAbstractListModel that exposes:
- Codec name (h264, h265, vp9, av1)
- Whether codec is enabled
- Whether hardware decoders are available
- Current mode (hardware/software)
- List of available decoders for current mode
- Selected decoder name

Roles: `CodecNameRole`, `EnabledRole`, `HwAvailableRole`, `IsHardwareRole`, `DecoderListRole`, `SelectedDecoderRole`

**Step 2: Wire to YamlConfig**

Model reads from and writes to:
- `video.codecs` — enabled codec list
- `video.decoder.<codec>` — selected decoder per codec
- `video.capabilities` — read-only, populated by installer/first-run

**Step 3: Build the QML UI**

In `VideoSettings.qml`, add a section after the existing resolution/fps controls:

```qml
// Video Decoding section
SectionHeader { text: "Video Decoding" }

Repeater {
    model: CodecCapabilityModel

    Column {
        width: parent.width
        spacing: 4

        Row {
            spacing: 8
            CheckBox {
                text: model.codecName.toUpperCase()
                checked: model.enabled
                enabled: model.codecName !== "h264"  // H.264 always on
                onToggled: model.enabled = checked
            }
            // Hardware/Software toggle
            RadioButton {
                text: "Hardware"
                enabled: model.hwAvailable
                checked: model.isHardware
                onClicked: model.isHardware = true
            }
            RadioButton {
                text: "Software"
                checked: !model.isHardware
                onClicked: model.isHardware = false
            }
        }

        ComboBox {
            width: 250
            model: decoderList
            currentIndex: decoderList.indexOf(model.selectedDecoder)
            onActivated: model.selectedDecoder = currentText
            visible: decoderList.length > 1
        }
    }
}
```

**Step 4: Register model in main.cpp**

```cpp
qmlRegisterSingletonType<CodecCapabilityModel>("OpenAutoProdigy", 1, 0, "CodecCapabilityModel", ...);
```

**Step 5: Build and test**

Run: `cd build && cmake --build . -j$(nproc)`
Manual test: verify settings page shows codec options on dev VM.

**Step 6: Commit**

```bash
git add src/ui/CodecCapabilityModel.hpp src/ui/CodecCapabilityModel.cpp \
        qml/applications/settings/VideoSettings.qml src/main.cpp src/CMakeLists.txt
git commit -m "feat: codec & decoder selection in Video Settings UI"
```

---

## Task 9: Video — Frame Pool for Software Decode Path

**Files:**
- Create: `src/core/aa/VideoFramePool.hpp`
- Modify: `src/core/aa/VideoDecoder.hpp`
- Modify: `src/core/aa/VideoDecoder.cpp:232-298`
- Modify: `src/CMakeLists.txt`
- Test: `tests/test_video_frame_pool.cpp`

**Context:** Currently, a fresh QVideoFrame is allocated per decode (~1.4MB at 720p, 30x/sec). A frame pool eliminates per-frame allocation in steady state.

**Step 1: Write the failing test**

Create `tests/test_video_frame_pool.cpp`:
```cpp
#include <QTest>
#include "core/aa/VideoFramePool.hpp"
#include <QVideoFrame>
#include <QVideoFrameFormat>

class TestVideoFramePool : public QObject {
    Q_OBJECT
private slots:
    void testAcquireReturnsValidFrame();
    void testPoolReusesBuffers();
    void testPoolGrowsWhenExhausted();
    void testFormatChange();
};

void TestVideoFramePool::testAcquireReturnsValidFrame()
{
    QVideoFrameFormat fmt({1280, 720}, QVideoFrameFormat::Format_YUV420P);
    oap::aa::VideoFramePool pool(fmt, 3);

    QVideoFrame frame = pool.acquire();
    QVERIFY(frame.isValid());
    QCOMPARE(frame.size(), QSize(1280, 720));
}

void TestVideoFramePool::testPoolReusesBuffers()
{
    QVideoFrameFormat fmt({1280, 720}, QVideoFrameFormat::Format_YUV420P);
    oap::aa::VideoFramePool pool(fmt, 3);

    // Acquire and immediately release 10 frames
    for (int i = 0; i < 10; ++i) {
        QVideoFrame frame = pool.acquire();
        QVERIFY(frame.isValid());
        // frame goes out of scope, should return to pool
    }
    // Pool shouldn't have allocated more than ~3-4 buffers total
    QVERIFY(pool.totalAllocated() <= 5);
}

void TestVideoFramePool::testPoolGrowsWhenExhausted()
{
    QVideoFrameFormat fmt({1280, 720}, QVideoFrameFormat::Format_YUV420P);
    oap::aa::VideoFramePool pool(fmt, 2);

    // Hold 5 frames simultaneously (exceeds pool size)
    std::vector<QVideoFrame> held;
    for (int i = 0; i < 5; ++i) {
        held.push_back(pool.acquire());
        QVERIFY(held.back().isValid());
    }
    // Should gracefully allocate more, not crash
    QVERIFY(pool.totalAllocated() >= 5);
}

void TestVideoFramePool::testFormatChange()
{
    QVideoFrameFormat fmt720({1280, 720}, QVideoFrameFormat::Format_YUV420P);
    oap::aa::VideoFramePool pool(fmt720, 3);
    pool.acquire();  // pre-populate

    QVideoFrameFormat fmt1080({1920, 1080}, QVideoFrameFormat::Format_YUV420P);
    pool.reset(fmt1080);

    QVideoFrame frame = pool.acquire();
    QCOMPARE(frame.size(), QSize(1920, 1080));
}

QTEST_MAIN(TestVideoFramePool)
#include "test_video_frame_pool.moc"
```

**Step 2: Implement VideoFramePool**

Create `src/core/aa/VideoFramePool.hpp`:

The pool manages raw byte buffers with return-on-destruct semantics. When a QVideoFrame is created from a pool buffer, the buffer returns to the pool when the last QVideoFrame copy is destroyed.

Key design:
- Pool stores `shared_ptr<PoolBuffer>` objects
- Each `PoolBuffer` has a `QByteArray` of the right size and a back-pointer to the pool
- `acquire()` checks for a free buffer; if none available, allocates a new one
- Thread-safe free list (mutex-protected — not on RT path, only on decoder worker thread + render thread destructor)

**Step 3: Integrate into VideoDecoder**

In `VideoDecoder.hpp`, add `VideoFramePool* framePool_` member.

In `VideoDecoder.cpp`, replace the QVideoFrame allocation block (lines 232-298):
- Use `framePool_->acquire()` instead of `QVideoFrame(format_)`
- On format change (resolution switch), call `framePool_->reset(newFormat)`
- Everything else (plane copy, marshal to main thread) stays the same

**Step 4: Build and run tests**

Run: `cd build && cmake .. && cmake --build . -j$(nproc) && ctest --output-on-failure`

**Step 5: Commit**

```bash
git add src/core/aa/VideoFramePool.hpp src/core/aa/VideoDecoder.hpp \
        src/core/aa/VideoDecoder.cpp tests/test_video_frame_pool.cpp \
        src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "perf: video frame pool eliminates per-frame QVideoFrame allocation"
```

---

## Task 10: FFmpeg Hardware Decoder Selection with Fallback

**Files:**
- Modify: `src/core/aa/VideoDecoder.hpp`
- Modify: `src/core/aa/VideoDecoder.cpp:27-66` (initCodec)
- Modify: `src/core/YamlConfig.hpp`
- Modify: `src/core/YamlConfig.cpp`

**Context:** VideoDecoder currently always uses software decoders. We need to try the user-configured decoder first (which may be hardware), with automatic fallback to software.

**Step 1: Add decoder config to YamlConfig**

In `YamlConfig.cpp` defaults:
```cpp
root_["video"]["decoder"]["h264"] = "auto";
root_["video"]["decoder"]["h265"] = "auto";
```

In `YamlConfig.hpp`:
```cpp
QString videoDecoder(const QString& codec) const;
```

**Step 2: Update initCodec() to try hw decoder first**

In `VideoDecoder.cpp`, modify `initCodec()`:

```cpp
bool VideoDecoder::initCodec(AVCodecID codecId)
{
    // Determine codec name for config lookup
    QString codecName = (codecId == AV_CODEC_ID_H264) ? "h264" : "h265";
    QString decoderPref = yamlConfig_ ? yamlConfig_->videoDecoder(codecName) : "auto";

    const AVCodec* codec = nullptr;

    // Try user-specified or auto-detected hw decoder first
    if (decoderPref != "auto") {
        codec = avcodec_find_decoder_by_name(decoderPref.toUtf8().constData());
        if (codec)
            qInfo() << "[VideoDecoder] Trying configured decoder:" << decoderPref;
    }

    if (!codec && decoderPref == "auto") {
        // Try known hw decoders in order
        static const char* hwDecoders264[] = {"h264_v4l2m2m", "h264_vaapi", nullptr};
        static const char* hwDecoders265[] = {"hevc_v4l2m2m", "hevc_vaapi", nullptr};
        const char** list = (codecId == AV_CODEC_ID_H264) ? hwDecoders264 : hwDecoders265;
        for (int i = 0; list[i]; ++i) {
            codec = avcodec_find_decoder_by_name(list[i]);
            if (codec) {
                qInfo() << "[VideoDecoder] Auto-detected hw decoder:" << list[i];
                break;
            }
        }
    }

    // Fallback to software
    if (!codec) {
        codec = avcodec_find_decoder(codecId);
        qInfo() << "[VideoDecoder] Using software decoder:" << codec->name;
    }

    // ... existing avcodec_alloc_context3, flags, avcodec_open2 ...

    // If avcodec_open2 fails with hw decoder, retry with software
    if (avcodec_open2(codecCtx_, codec, nullptr) < 0) {
        qWarning() << "[VideoDecoder]" << codec->name << "failed to open, falling back to software";
        avcodec_free_context(&codecCtx_);
        codec = avcodec_find_decoder(codecId);
        codecCtx_ = avcodec_alloc_context3(codec);
        // ... re-set flags ...
        if (avcodec_open2(codecCtx_, codec, nullptr) < 0) {
            qCritical() << "[VideoDecoder] Software decoder also failed!";
            return false;
        }
    }

    usingHardware_ = (codec->capabilities & AV_CODEC_CAP_HARDWARE) != 0;
    qInfo() << "[VideoDecoder] Initialized:" << codec->name
            << (usingHardware_ ? "(hardware)" : "(software)");
    return true;
}
```

**Step 3: Add first-frame fallback**

In `processFrame()`, after the first `avcodec_receive_frame()`, if it fails and we're using a hw decoder, reinitialize with software:

```cpp
if (ret < 0 && usingHardware_ && !firstFrameDecoded_) {
    qWarning() << "[VideoDecoder] First frame decode failed on hw decoder, falling back";
    initCodec(activeCodecId_);  // will retry with software
    // Re-send the packet to the new codec context
    avcodec_send_packet(codecCtx_, packet_);
    ret = avcodec_receive_frame(codecCtx_, frame_);
}
firstFrameDecoded_ = true;
```

**Step 4: Build and test**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`

**Step 5: Commit**

```bash
git add src/core/aa/VideoDecoder.hpp src/core/aa/VideoDecoder.cpp \
        src/core/YamlConfig.hpp src/core/YamlConfig.cpp
git commit -m "feat: FFmpeg hw decoder selection with automatic software fallback"
```

---

## Task 11: Hardware QAbstractVideoBuffer (Qt 6.8+ only)

**Files:**
- Create: `src/core/aa/DmaBufVideoBuffer.hpp`
- Create: `src/core/aa/DmaBufVideoBuffer.cpp`
- Modify: `src/core/aa/VideoDecoder.cpp` (add DRM_PRIME frame path)
- Modify: `src/CMakeLists.txt`

**Context:** When FFmpeg outputs DRM_PRIME frames (hardware decode), wrap the dmabuf fd in a custom QAbstractVideoBuffer for zero-copy rendering. This is Qt 6.8+ only.

**Step 1: Gate with Qt version check**

All code in this task is wrapped in:
```cpp
#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
// ... DRM_PRIME path ...
#endif
```

**Step 2: Implement DmaBufVideoBuffer**

Create `src/core/aa/DmaBufVideoBuffer.hpp`:
```cpp
#pragma once
#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)

#include <QAbstractVideoBuffer>
#include <QVideoFrame>
extern "C" {
#include <libavutil/hwcontext_drm.h>
}

namespace oap {
namespace aa {

/// Wraps a DRM_PRIME dmabuf fd for zero-copy rendering via Qt's RHI.
/// Falls back to CPU mapping if RHI texture import is not available.
class DmaBufVideoBuffer : public QAbstractVideoBuffer
{
public:
    DmaBufVideoBuffer(AVFrame* frame, AVBufferRef* hwFrameRef);
    ~DmaBufVideoBuffer() override;

    MapData map(QVideoFrame::MapMode mode) override;
    void unmap() override;

private:
    AVFrame* frame_;           // kept alive for dmabuf fd lifetime
    AVBufferRef* hwFrameRef_;  // reference to hw frames context
    AVFrame* swFrame_ = nullptr;  // CPU fallback frame (lazy allocated)
    bool mapped_ = false;
};

} // namespace aa
} // namespace oap

#endif // QT_VERSION >= 6.8.0
```

**Step 3: Integrate into VideoDecoder processFrame()**

In the decode path, after `avcodec_receive_frame()`:
```cpp
#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
if (frame_->format == AV_PIX_FMT_DRM_PRIME) {
    // Zero-copy hardware path
    auto* buffer = new DmaBufVideoBuffer(frame_, codecCtx_->hw_frames_ctx);
    QVideoFrame videoFrame(buffer, format_);
    // ... marshal to main thread same as software path ...
}
else
#endif
{
    // Existing software path (with frame pool optimization from Task 9)
}
```

**Step 4: Build and test**

On dev VM (Qt 6.4): ifdef'd out, build should succeed with no changes.
On Pi (Qt 6.8): builds and links. Actual testing requires hw decode enabled.

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`

**Step 5: Commit**

```bash
git add src/core/aa/DmaBufVideoBuffer.hpp src/core/aa/DmaBufVideoBuffer.cpp \
        src/core/aa/VideoDecoder.cpp src/CMakeLists.txt
git commit -m "feat: zero-copy DRM_PRIME video frames via QAbstractVideoBuffer (Qt 6.8+)"
```

---

## Task 12: Integration Testing & Documentation

**Files:**
- Modify: `docs/plans/2026-02-25-av-pipeline-optimization-design.md` (mark complete)
- Modify: `CLAUDE.md` (update current status)

**Step 1: Full build and test on dev VM**

```bash
cd build && cmake .. && cmake --build . -j$(nproc) && ctest --output-on-failure
```

All tests must pass.

**Step 2: Cross-compile and deploy to Pi**

```bash
docker run --rm -u "$(id -u):$(id -g)" -v "$(pwd):/src:rw" -w /src/build-pi \
    openauto-cross-pi4 cmake --build . -j$(nproc)
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/
```

**Step 3: Test on Pi**

1. Launch app, connect phone
2. Verify video renders at configured resolution
3. Check logs for decoder selection: `[VideoDecoder] Using h264_v4l2m2m (hardware)` or software fallback
4. Change resolution in settings — verify touch + video reconnect correctly
5. Check audio: no underruns at default buffer sizes, verify lip-sync improvement
6. Check logs for ring buffer sizing: `[Audio] Buffer: media=50ms speech=35ms system=35ms`

**Step 4: Update CLAUDE.md**

Add to Known limitations / TODO section:
- DmaBufVideoBuffer (Qt 6.8 zero-copy) needs real-world testing on Pi with v4l2m2m
- Adaptive audio buffer growth needs stress testing under load

**Step 5: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: update status after AV pipeline optimization"
```

---

## Verification Checklist

1. **All tests pass:** `ctest --test-dir build --output-on-failure` — zero failures (except pre-existing test_tcp_transport)
2. **Cross-compile succeeds:** `docker run ... cmake --build . -j$(nproc)` — zero errors
3. **Binary runs on Pi:** Video renders, audio plays, no crashes
4. **Audio latency reduced:** Ring buffer sizes in logs show 35-50ms instead of 100ms
5. **Codec detection works:** Startup logs show detected decoders
6. **Settings UI functional:** Codec toggles and decoder dropdowns work
7. **No stale references:** `grep -r "0\.1" src/core/services/AudioService.cpp` — the old 100ms multiplier is gone
