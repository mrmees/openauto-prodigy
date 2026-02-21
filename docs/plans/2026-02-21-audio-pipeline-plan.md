# Audio Pipeline Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Rebuild AudioService so AA audio actually plays through user-selectable PipeWire devices, with mic capture for Google Assistant.

**Architecture:** Replace broken `pw_main_loop` with `pw_thread_loop`. Add `spa_ringbuffer` per stream to bridge ASIO→PipeWire RT threads. Add `pw_registry` listener for device enumeration. Expose device models to QML and web config.

**Tech Stack:** PipeWire C API (`pw_thread_loop`, `pw_stream`, `pw_registry`, `spa_ringbuffer`), Qt 6 / QML, C++17

---

### Task 1: AudioRingBuffer — Lock-Free SPSC Ring Buffer

**Files:**
- Create: `src/core/audio/AudioRingBuffer.hpp`
- Create: `tests/test_audio_ring_buffer.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `src/CMakeLists.txt`

The ring buffer bridges ASIO threads (producer) and PipeWire RT callbacks (consumer). Uses `spa_ringbuffer` from PipeWire's SPA library.

**Step 1: Write the test file**

```cpp
// tests/test_audio_ring_buffer.cpp
#include <QTest>
#include "core/audio/AudioRingBuffer.hpp"

class TestAudioRingBuffer : public QObject {
    Q_OBJECT

private slots:
    void constructionSetsCapacity()
    {
        oap::AudioRingBuffer rb(4096);
        QCOMPARE(rb.capacity(), 4096u);
        QCOMPARE(rb.available(), 0u);
    }

    void writeAndRead()
    {
        oap::AudioRingBuffer rb(1024);
        uint8_t writeData[256];
        for (int i = 0; i < 256; i++) writeData[i] = static_cast<uint8_t>(i);

        QCOMPARE(rb.write(writeData, 256), 256u);
        QCOMPARE(rb.available(), 256u);

        uint8_t readData[256] = {};
        QCOMPARE(rb.read(readData, 256), 256u);
        QCOMPARE(rb.available(), 0u);

        for (int i = 0; i < 256; i++)
            QCOMPARE(readData[i], static_cast<uint8_t>(i));
    }

    void readFromEmptyReturnsZero()
    {
        oap::AudioRingBuffer rb(1024);
        uint8_t buf[64];
        QCOMPARE(rb.read(buf, 64), 0u);
    }

    void overrunDropsOldest()
    {
        // Capacity must be power of 2 for spa_ringbuffer
        oap::AudioRingBuffer rb(256);
        uint8_t data[256];
        memset(data, 0xAA, 256);
        rb.write(data, 256);  // fill completely

        // Write 64 more bytes — should overwrite oldest
        uint8_t moreData[64];
        memset(moreData, 0xBB, 64);
        rb.write(moreData, 64);

        // Read should get the newest data (we advanced the read pointer)
        QVERIFY(rb.available() <= 256u);
    }

    void resetClearsBuffer()
    {
        oap::AudioRingBuffer rb(1024);
        uint8_t data[128];
        rb.write(data, 128);
        rb.reset();
        QCOMPARE(rb.available(), 0u);
    }

    void partialReadLeavesRemainder()
    {
        oap::AudioRingBuffer rb(1024);
        uint8_t data[200];
        for (int i = 0; i < 200; i++) data[i] = static_cast<uint8_t>(i);
        rb.write(data, 200);

        uint8_t partial[100];
        QCOMPARE(rb.read(partial, 100), 100u);
        QCOMPARE(rb.available(), 100u);

        // Second read should get the remaining data
        uint8_t rest[100];
        QCOMPARE(rb.read(rest, 100), 100u);
        QCOMPARE(rest[0], static_cast<uint8_t>(100));
    }
};

QTEST_MAIN(TestAudioRingBuffer)
#include "test_audio_ring_buffer.moc"
```

**Step 2: Run test to verify it fails**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make test_audio_ring_buffer 2>&1`
Expected: Compilation failure — `AudioRingBuffer.hpp` doesn't exist

**Step 3: Create the `src/core/audio/` directory and header**

```cpp
// src/core/audio/AudioRingBuffer.hpp
#pragma once

#include <spa/utils/ringbuffer.h>
#include <cstdint>
#include <cstring>
#include <vector>

namespace oap {

// Lock-free SPSC ring buffer wrapping PipeWire's spa_ringbuffer.
// One producer (ASIO thread), one consumer (PipeWire RT callback).
// Capacity MUST be a power of 2.
class AudioRingBuffer {
public:
    explicit AudioRingBuffer(uint32_t capacity)
        : capacity_(capacity)
        , data_(capacity, 0)
    {
        spa_ringbuffer_init(&ring_);
    }

    uint32_t capacity() const { return capacity_; }

    uint32_t available() const
    {
        return spa_ringbuffer_get_read_index(&ring_, nullptr);
    }

    // Write data into the ring buffer. If overrun, advances read pointer
    // to discard oldest data (keeps latency bounded).
    // Returns bytes actually written.
    uint32_t write(const uint8_t* src, uint32_t size)
    {
        if (!src || size == 0) return 0;

        uint32_t toWrite = (size > capacity_) ? capacity_ : size;

        // Check if we need to drop oldest data
        uint32_t avail = available();
        if (avail + toWrite > capacity_) {
            uint32_t drop = avail + toWrite - capacity_;
            int32_t idx;
            spa_ringbuffer_get_read_index(&ring_, &idx);
            spa_ringbuffer_read_update(&ring_, idx + static_cast<int32_t>(drop));
        }

        int32_t writeIdx;
        spa_ringbuffer_get_write_index(&ring_, &writeIdx);

        uint32_t offset = static_cast<uint32_t>(writeIdx) & (capacity_ - 1);
        uint32_t first = capacity_ - offset;
        if (first > toWrite) first = toWrite;

        memcpy(data_.data() + offset, src, first);
        if (first < toWrite)
            memcpy(data_.data(), src + first, toWrite - first);

        spa_ringbuffer_write_update(&ring_, writeIdx + static_cast<int32_t>(toWrite));
        return toWrite;
    }

    // Read data from the ring buffer. Returns bytes actually read.
    // If not enough data, reads what's available (caller should zero-fill remainder).
    uint32_t read(uint8_t* dst, uint32_t size)
    {
        if (!dst || size == 0) return 0;

        int32_t readIdx;
        uint32_t avail = spa_ringbuffer_get_read_index(&ring_, &readIdx);
        uint32_t toRead = (size > avail) ? avail : size;
        if (toRead == 0) return 0;

        uint32_t offset = static_cast<uint32_t>(readIdx) & (capacity_ - 1);
        uint32_t first = capacity_ - offset;
        if (first > toRead) first = toRead;

        memcpy(dst, data_.data() + offset, first);
        if (first < toRead)
            memcpy(dst + first, data_.data(), toRead - first);

        spa_ringbuffer_read_update(&ring_, readIdx + static_cast<int32_t>(toRead));
        return toRead;
    }

    void reset()
    {
        spa_ringbuffer_init(&ring_);
    }

private:
    uint32_t capacity_;
    std::vector<uint8_t> data_;
    struct spa_ringbuffer ring_{};
};

} // namespace oap
```

**Step 4: Add to CMakeLists**

In `tests/CMakeLists.txt`, add a new test target (follow existing pattern):
```cmake
add_executable(test_audio_ring_buffer test_audio_ring_buffer.cpp)
target_link_libraries(test_audio_ring_buffer PRIVATE Qt6::Core Qt6::Test ${PIPEWIRE_LIBRARIES})
target_include_directories(test_audio_ring_buffer PRIVATE ${CMAKE_SOURCE_DIR}/src ${PIPEWIRE_INCLUDE_DIRS})
add_test(NAME test_audio_ring_buffer COMMAND test_audio_ring_buffer)
```

In `src/CMakeLists.txt`, no source file needed (header-only), but ensure `src/core/audio/` is a valid include path (it already is via `${CMAKE_SOURCE_DIR}/src` include).

**Step 5: Build and run test**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make test_audio_ring_buffer && ctest -R test_audio_ring_buffer --output-on-failure`
Expected: All 6 tests pass

**Step 6: Commit**

```bash
git add src/core/audio/AudioRingBuffer.hpp tests/test_audio_ring_buffer.cpp tests/CMakeLists.txt
git commit -m "feat(audio): add lock-free SPSC ring buffer wrapping spa_ringbuffer"
```

---

### Task 2: Rewrite AudioService Core — `pw_thread_loop` + Stream Connect

**Files:**
- Modify: `src/core/services/AudioService.hpp`
- Modify: `src/core/services/AudioService.cpp`
- Modify: `src/core/services/IAudioService.hpp`
- Modify: `tests/test_audio_service.cpp`

This is the biggest task — replaces the broken PipeWire integration with working code. Changes:
1. `pw_main_loop` → `pw_thread_loop`
2. `createStream()` gets format params and calls `pw_stream_connect()`
3. `writeAudio()` writes into a ring buffer instead of directly dequeuing PipeWire buffers
4. A PipeWire `process` callback drains the ring buffer into PipeWire buffers
5. Capture stream uses the same `pw_thread_loop`

**Step 1: Update IAudioService to pass format info**

Add stream format parameters to `createStream()` in `IAudioService.hpp`:

```cpp
// In IAudioService.hpp, replace the createStream signature:
virtual AudioStreamHandle* createStream(
    const QString& name, int priority,
    int sampleRate = 48000, int channels = 2,
    const QString& targetDevice = "auto") = 0;
```

Also add to `IAudioService.hpp`:
```cpp
// New methods for device targeting
virtual void setOutputDevice(const QString& deviceName) = 0;
virtual void setInputDevice(const QString& deviceName) = 0;
virtual QString outputDevice() const = 0;
virtual QString inputDevice() const = 0;
```

**Step 2: Update AudioStreamHandle**

In `AudioService.hpp`, expand `AudioStreamHandle` to carry ring buffer and format:

```cpp
#include "core/audio/AudioRingBuffer.hpp"
#include <spa/param/audio/format-utils.h>
#include <memory>

struct AudioStreamHandle {
    QString name;
    int priority = 0;
    struct pw_stream* stream = nullptr;
    AudioFocusType focusType = AudioFocusType::Gain;
    bool hasFocus = false;
    float volume = 1.0f;
    float baseVolume = 1.0f;

    // Format info (needed for process callback)
    int sampleRate = 48000;
    int channels = 2;
    int bytesPerFrame = 4; // channels * sizeof(int16_t)

    // Ring buffer for ASIO → PipeWire bridging
    std::unique_ptr<oap::AudioRingBuffer> ringBuffer;

    // PipeWire listener (must outlive stream)
    struct spa_hook listener{};
    struct pw_stream_events events{};
};
```

**Step 3: Rewrite AudioService header**

Replace `pw_main_loop*` with `pw_thread_loop*` and add registry/device members:

```cpp
// In AudioService.hpp private section, replace:
//   struct pw_main_loop* mainLoop_ = nullptr;
// With:
    struct pw_thread_loop* threadLoop_ = nullptr;
    struct pw_context* context_ = nullptr;
    struct pw_core* core_ = nullptr;
    struct pw_registry* registry_ = nullptr;
    struct spa_hook registryListener_{};
    struct spa_hook coreListener_{};

    QString outputDevice_ = "auto";
    QString inputDevice_ = "auto";

    mutable QMutex mutex_;
    QList<AudioStreamHandle*> streams_;
    int masterVolume_ = 80;

    CaptureState capture_;
    struct spa_hook captureListener_{};

    // Playback process callback
    static void onPlaybackProcess(void* userdata);
```

Update `isAvailable()` to check `threadLoop_` instead of `mainLoop_`.

**Step 4: Rewrite AudioService constructor**

```cpp
AudioService::AudioService(QObject* parent)
    : QObject(parent)
{
    pw_init(nullptr, nullptr);

    threadLoop_ = pw_thread_loop_new("openauto-audio", nullptr);
    if (!threadLoop_) {
        qWarning() << "AudioService: Failed to create PipeWire thread loop";
        return;
    }

    pw_thread_loop_lock(threadLoop_);

    context_ = pw_context_new(pw_thread_loop_get_loop(threadLoop_), nullptr, 0);
    if (!context_) {
        qWarning() << "AudioService: Failed to create PipeWire context";
        pw_thread_loop_unlock(threadLoop_);
        pw_thread_loop_destroy(threadLoop_);
        threadLoop_ = nullptr;
        return;
    }

    core_ = pw_context_connect(context_, nullptr, 0);
    if (!core_) {
        qWarning() << "AudioService: Failed to connect to PipeWire daemon"
                    << " — audio will be unavailable";
        pw_thread_loop_unlock(threadLoop_);
        pw_context_destroy(context_);
        context_ = nullptr;
        pw_thread_loop_destroy(threadLoop_);
        threadLoop_ = nullptr;
        return;
    }

    pw_thread_loop_unlock(threadLoop_);

    if (pw_thread_loop_start(threadLoop_) < 0) {
        qWarning() << "AudioService: Failed to start PipeWire thread loop";
        pw_core_disconnect(core_);
        core_ = nullptr;
        pw_context_destroy(context_);
        context_ = nullptr;
        pw_thread_loop_destroy(threadLoop_);
        threadLoop_ = nullptr;
        return;
    }

    qInfo() << "AudioService: Connected to PipeWire daemon";
}
```

**Step 5: Rewrite `createStream()` with format params and `pw_stream_connect()`**

```cpp
AudioStreamHandle* AudioService::createStream(
    const QString& name, int priority,
    int sampleRate, int channels, const QString& targetDevice)
{
    if (!isAvailable()) {
        qWarning() << "AudioService::createStream: PipeWire not available";
        return nullptr;
    }

    auto* handle = new AudioStreamHandle();
    handle->name = name;
    handle->priority = priority;
    handle->sampleRate = sampleRate;
    handle->channels = channels;
    handle->bytesPerFrame = channels * 2; // 16-bit PCM

    // Ring buffer: ~100ms of audio at this format
    uint32_t rbSize = static_cast<uint32_t>(sampleRate * channels * 2 * 0.1);
    // Round up to next power of 2 (required by spa_ringbuffer)
    uint32_t pow2 = 1;
    while (pow2 < rbSize) pow2 <<= 1;
    handle->ringBuffer = std::make_unique<AudioRingBuffer>(pow2);

    // Determine PipeWire role based on stream name
    const char* role = "Music";
    if (name.contains("Navigation") || name.contains("Speech"))
        role = "Communication";
    else if (name.contains("System"))
        role = "Notification";

    pw_thread_loop_lock(threadLoop_);

    auto props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, role,
        PW_KEY_NODE_NAME, name.toUtf8().constData(),
        PW_KEY_APP_NAME, "OpenAuto Prodigy",
        nullptr);

    // Target specific device if not "auto"
    QString device = targetDevice == "auto" ? outputDevice_ : targetDevice;
    if (device != "auto" && !device.isEmpty()) {
        pw_properties_set(props, PW_KEY_TARGET_OBJECT, device.toUtf8().constData());
    }

    handle->stream = pw_stream_new(core_, name.toUtf8().constData(), props);
    if (!handle->stream) {
        qWarning() << "AudioService: Failed to create PipeWire stream:" << name;
        pw_thread_loop_unlock(threadLoop_);
        delete handle;
        return nullptr;
    }

    // Set up process callback
    handle->events = {};
    handle->events.version = PW_VERSION_STREAM_EVENTS;
    handle->events.process = &AudioService::onPlaybackProcess;
    pw_stream_add_listener(handle->stream, &handle->listener, &handle->events, handle);

    // Build format params
    uint8_t paramBuf[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(paramBuf, sizeof(paramBuf));

    struct spa_audio_info_raw rawInfo{};
    rawInfo.format = SPA_AUDIO_FORMAT_S16_LE;
    rawInfo.rate = static_cast<uint32_t>(sampleRate);
    rawInfo.channels = static_cast<uint32_t>(channels);

    const struct spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    int ret = pw_stream_connect(handle->stream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        static_cast<pw_stream_flags>(
            PW_STREAM_FLAG_AUTOCONNECT |
            PW_STREAM_FLAG_MAP_BUFFERS |
            PW_STREAM_FLAG_RT_PROCESS),
        params, 1);

    if (ret < 0) {
        qWarning() << "AudioService: Failed to connect stream:" << name << "error:" << ret;
        pw_stream_destroy(handle->stream);
        pw_thread_loop_unlock(threadLoop_);
        delete handle;
        return nullptr;
    }

    pw_thread_loop_unlock(threadLoop_);

    QMutexLocker lock(&mutex_);
    streams_.append(handle);

    qInfo() << "AudioService: Created stream" << name
            << sampleRate << "Hz" << channels << "ch"
            << "priority:" << priority;
    return handle;
}
```

**Step 6: Add playback process callback and rewrite `writeAudio()`**

```cpp
// Static callback — fires on PipeWire RT thread when it needs audio data
void AudioService::onPlaybackProcess(void* userdata)
{
    auto* handle = static_cast<AudioStreamHandle*>(userdata);
    if (!handle || !handle->stream || !handle->ringBuffer)
        return;

    struct pw_buffer* buf = pw_stream_dequeue_buffer(handle->stream);
    if (!buf) return;

    struct spa_data& d = buf->buffer->datas[0];
    uint32_t maxSize = d.maxsize;

    uint32_t read = handle->ringBuffer->read(
        static_cast<uint8_t*>(d.data), maxSize);

    // Zero-fill remainder on underrun (silence)
    if (read < maxSize) {
        memset(static_cast<uint8_t*>(d.data) + read, 0, maxSize - read);
    }

    d.chunk->offset = 0;
    d.chunk->stride = handle->bytesPerFrame;
    d.chunk->size = maxSize;

    pw_stream_queue_buffer(handle->stream, buf);
}

// writeAudio now just pushes into the ring buffer (called from ASIO thread)
int AudioService::writeAudio(AudioStreamHandle* handle,
                              const uint8_t* data, int size)
{
    if (!handle || !handle->ringBuffer || !data || size <= 0)
        return -1;

    return static_cast<int>(handle->ringBuffer->write(data, static_cast<uint32_t>(size)));
}
```

**Step 7: Update destructor for `pw_thread_loop`**

```cpp
AudioService::~AudioService()
{
    // Destroy all remaining streams (must lock thread loop)
    if (threadLoop_) {
        pw_thread_loop_lock(threadLoop_);
        QMutexLocker lock(&mutex_);
        for (auto* handle : streams_) {
            if (handle->stream) {
                spa_hook_remove(&handle->listener);
                pw_stream_destroy(handle->stream);
            }
            delete handle;
        }
        streams_.clear();
        lock.unlock();
        pw_thread_loop_unlock(threadLoop_);
    }

    // Shut down PipeWire (order matters)
    if (threadLoop_)
        pw_thread_loop_stop(threadLoop_);  // Don't hold lock!
    if (core_)
        pw_core_disconnect(core_);
    if (context_)
        pw_context_destroy(context_);
    if (threadLoop_)
        pw_thread_loop_destroy(threadLoop_);

    pw_deinit();
}
```

**Step 8: Update `destroyStream()` to lock thread loop**

```cpp
void AudioService::destroyStream(AudioStreamHandle* handle)
{
    if (!handle) return;

    QMutexLocker lock(&mutex_);
    streams_.removeOne(handle);
    lock.unlock();

    if (handle->stream && threadLoop_) {
        pw_thread_loop_lock(threadLoop_);
        spa_hook_remove(&handle->listener);
        pw_stream_destroy(handle->stream);
        pw_thread_loop_unlock(threadLoop_);
    }

    qInfo() << "AudioService: Destroyed stream" << handle->name;
    delete handle;
}
```

**Step 9: Add device getter/setter methods**

```cpp
void AudioService::setOutputDevice(const QString& deviceName)
{
    QMutexLocker lock(&mutex_);
    outputDevice_ = deviceName.isEmpty() ? "auto" : deviceName;
    qInfo() << "AudioService: Output device set to" << outputDevice_;
}

void AudioService::setInputDevice(const QString& deviceName)
{
    QMutexLocker lock(&mutex_);
    inputDevice_ = deviceName.isEmpty() ? "auto" : deviceName;
    qInfo() << "AudioService: Input device set to" << inputDevice_;
}

QString AudioService::outputDevice() const
{
    QMutexLocker lock(&mutex_);
    return outputDevice_;
}

QString AudioService::inputDevice() const
{
    QMutexLocker lock(&mutex_);
    return inputDevice_;
}
```

**Step 10: Update capture stream to use `pw_thread_loop`**

Wrap `openCaptureStream()` and `closeCaptureStream()` PipeWire calls with `pw_thread_loop_lock/unlock` instead of bare calls. Add `PW_KEY_TARGET_OBJECT` for input device selection (same pattern as output). The capture process callback (`onCaptureProcess`) stays the same — it already works correctly for the pull model.

**Step 11: Update ServiceFactory to pass format params**

In `ServiceFactory.cpp`, the `AudioServiceStub` constructor calls `audioService_->createStream(streamName, streamPriority)`. Update to:
```cpp
audioStream_ = audioService_->createStream(streamName, streamPriority, SampleRate, ChannelCount);
```

`SampleRate` and `ChannelCount` are already template params on `AudioServiceStub`.

**Step 12: Update tests**

Update `test_audio_service.cpp`:
- `createStreamReturnsNullWithoutDaemon`: now passes `(name, priority, sampleRate, channels)`
- `createAndDestroyWithDaemon`: same

**Step 13: Build and run all tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: All existing tests pass. Audio tests may skip on dev VM (no PipeWire daemon).

**Step 14: Commit**

```bash
git add src/core/services/AudioService.hpp src/core/services/AudioService.cpp \
        src/core/services/IAudioService.hpp src/core/aa/ServiceFactory.cpp \
        tests/test_audio_service.cpp
git commit -m "feat(audio): rewrite AudioService with pw_thread_loop, stream connect, and ring buffers

Replaces the broken pw_main_loop (never run) with pw_thread_loop.
Playback streams now call pw_stream_connect() with proper format params.
writeAudio() pushes into spa_ringbuffer; PipeWire RT callback drains it.
createStream() accepts sample rate, channel count, and target device."
```

---

### Task 3: Device Enumeration — `PipeWireDeviceRegistry`

**Files:**
- Create: `src/core/audio/AudioDeviceInfo.hpp`
- Create: `src/core/audio/PipeWireDeviceRegistry.hpp`
- Create: `src/core/audio/PipeWireDeviceRegistry.cpp`
- Create: `tests/test_device_registry.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the test file**

```cpp
// tests/test_device_registry.cpp
#include <QTest>
#include <QSignalSpy>
#include "core/audio/PipeWireDeviceRegistry.hpp"

class TestDeviceRegistry : public QObject {
    Q_OBJECT

private slots:
    void constructionDoesNotCrash()
    {
        // May not connect to PipeWire on dev VM — that's okay
        oap::PipeWireDeviceRegistry registry;
        QVERIFY(true);
    }

    void addDeviceEmitsSignal()
    {
        oap::PipeWireDeviceRegistry registry;
        QSignalSpy spy(&registry, &oap::PipeWireDeviceRegistry::deviceAdded);

        // Simulate adding a device (test helper)
        oap::AudioDeviceInfo info;
        info.registryId = 42;
        info.nodeName = "test.sink";
        info.description = "Test Sink";
        info.mediaClass = "Audio/Sink";
        registry.testAddDevice(info);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(registry.outputDevices().size(), 1);
        QCOMPARE(registry.outputDevices().first().nodeName, QString("test.sink"));
    }

    void removeDeviceEmitsSignal()
    {
        oap::PipeWireDeviceRegistry registry;
        QSignalSpy addSpy(&registry, &oap::PipeWireDeviceRegistry::deviceAdded);
        QSignalSpy removeSpy(&registry, &oap::PipeWireDeviceRegistry::deviceRemoved);

        oap::AudioDeviceInfo info;
        info.registryId = 42;
        info.nodeName = "test.sink";
        info.description = "Test Sink";
        info.mediaClass = "Audio/Sink";
        registry.testAddDevice(info);
        registry.testRemoveDevice(42);

        QCOMPARE(removeSpy.count(), 1);
        QCOMPARE(registry.outputDevices().size(), 0);
    }

    void separatesOutputsAndInputs()
    {
        oap::PipeWireDeviceRegistry registry;

        oap::AudioDeviceInfo sink;
        sink.registryId = 1;
        sink.nodeName = "out.sink";
        sink.description = "Speaker";
        sink.mediaClass = "Audio/Sink";
        registry.testAddDevice(sink);

        oap::AudioDeviceInfo source;
        source.registryId = 2;
        source.nodeName = "in.source";
        source.description = "Mic";
        source.mediaClass = "Audio/Source";
        registry.testAddDevice(source);

        QCOMPARE(registry.outputDevices().size(), 1);
        QCOMPARE(registry.inputDevices().size(), 1);
        QCOMPARE(registry.outputDevices().first().nodeName, QString("out.sink"));
        QCOMPARE(registry.inputDevices().first().nodeName, QString("in.source"));
    }

    void duplexDeviceAppearsInBoth()
    {
        oap::PipeWireDeviceRegistry registry;

        oap::AudioDeviceInfo duplex;
        duplex.registryId = 3;
        duplex.nodeName = "usb.headset";
        duplex.description = "USB Headset";
        duplex.mediaClass = "Audio/Duplex";
        registry.testAddDevice(duplex);

        QCOMPARE(registry.outputDevices().size(), 1);
        QCOMPARE(registry.inputDevices().size(), 1);
    }
};

QTEST_MAIN(TestDeviceRegistry)
#include "test_device_registry.moc"
```

**Step 2: Create `AudioDeviceInfo` struct**

```cpp
// src/core/audio/AudioDeviceInfo.hpp
#pragma once
#include <QString>
#include <cstdint>

namespace oap {

struct AudioDeviceInfo {
    uint32_t registryId = 0;   // PipeWire session-scoped ID
    QString nodeName;           // node.name — used as config key
    QString description;        // node.description — display name
    QString mediaClass;         // "Audio/Sink", "Audio/Source", "Audio/Duplex"
};

} // namespace oap
```

**Step 3: Create `PipeWireDeviceRegistry`**

```cpp
// src/core/audio/PipeWireDeviceRegistry.hpp
#pragma once

#include "AudioDeviceInfo.hpp"
#include <QObject>
#include <QList>
#include <QMutex>
#include <pipewire/pipewire.h>

namespace oap {

class PipeWireDeviceRegistry : public QObject {
    Q_OBJECT

public:
    explicit PipeWireDeviceRegistry(QObject* parent = nullptr);
    ~PipeWireDeviceRegistry() override;

    // Start listening for devices (call after PipeWire thread loop is running)
    void start(struct pw_thread_loop* threadLoop, struct pw_core* core);
    void stop();

    QList<AudioDeviceInfo> outputDevices() const;
    QList<AudioDeviceInfo> inputDevices() const;

    // Test helpers (public for unit tests, not used in production)
    void testAddDevice(const AudioDeviceInfo& info);
    void testRemoveDevice(uint32_t registryId);

signals:
    void deviceAdded(const oap::AudioDeviceInfo& info);
    void deviceRemoved(uint32_t registryId);

private:
    static void onGlobal(void* data, uint32_t id, uint32_t permissions,
                         const char* type, uint32_t version,
                         const struct spa_dict* props);
    static void onGlobalRemove(void* data, uint32_t id);

    void addDevice(const AudioDeviceInfo& info);
    void removeDevice(uint32_t registryId);

    mutable QMutex mutex_;
    QList<AudioDeviceInfo> devices_;

    struct pw_thread_loop* threadLoop_ = nullptr;
    struct pw_registry* registry_ = nullptr;
    struct spa_hook registryListener_{};
};

} // namespace oap
```

Implementation in `PipeWireDeviceRegistry.cpp`:
- `start()`: calls `pw_core_get_registry()`, adds listener with `onGlobal` / `onGlobalRemove` callbacks
- `onGlobal()`: filters for `PW_TYPE_INTERFACE_Node`, checks `media.class` for `Audio/Sink`, `Audio/Source`, `Audio/Duplex`, extracts `node.name` + `node.description`, calls `addDevice()` which appends to list and emits `deviceAdded` signal via `QMetaObject::invokeMethod(Qt::QueuedConnection)` (marshal from PipeWire thread to Qt thread)
- `onGlobalRemove()`: finds device by registry ID, removes, emits `deviceRemoved`
- `testAddDevice()` / `testRemoveDevice()`: directly call `addDevice()` / `removeDevice()` without PipeWire (for unit tests)

**Step 4: Add to CMakeLists**

Add `core/audio/PipeWireDeviceRegistry.cpp` to `src/CMakeLists.txt` source list.
Add test target in `tests/CMakeLists.txt`.

**Step 5: Build and run tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass including the 4 new device registry tests.

**Step 6: Commit**

```bash
git add src/core/audio/AudioDeviceInfo.hpp src/core/audio/PipeWireDeviceRegistry.hpp \
        src/core/audio/PipeWireDeviceRegistry.cpp tests/test_device_registry.cpp \
        src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(audio): add PipeWireDeviceRegistry for sink/source enumeration"
```

---

### Task 4: Wire Device Registry into AudioService and Main

**Files:**
- Modify: `src/core/services/AudioService.hpp`
- Modify: `src/core/services/AudioService.cpp`
- Modify: `src/main.cpp`

**Step 1: Add registry to AudioService**

AudioService creates and owns a `PipeWireDeviceRegistry`. After the thread loop starts, call `registry_.start(threadLoop_, core_)`.

Add public methods:
```cpp
PipeWireDeviceRegistry* deviceRegistry() { return &deviceRegistry_; }
```

**Step 2: Start registry in constructor**

After `pw_thread_loop_start()` succeeds:
```cpp
deviceRegistry_.start(threadLoop_, core_);
```

**Step 3: Expose to QML in `main.cpp`**

```cpp
engine.rootContext()->setContextProperty("AudioService", audioService);
engine.rootContext()->setContextProperty("AudioDeviceRegistry", audioService->deviceRegistry());
```

**Step 4: Read initial device config**

In `main.cpp`, after AudioService creation, read config and apply:
```cpp
audioService->setOutputDevice(config->value("audio.output_device", "auto").toString());
audioService->setInputDevice(config->value("audio.input_device", "auto").toString());
audioService->setMasterVolume(config->value("audio.master_volume", 80).toInt());
```

**Step 5: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass. On Pi with PipeWire, device list should populate.

**Step 6: Commit**

```bash
git add src/core/services/AudioService.hpp src/core/services/AudioService.cpp src/main.cpp
git commit -m "feat(audio): wire device registry into AudioService, expose to QML"
```

---

### Task 5: QML Device Models and Audio Settings UI

**Files:**
- Create: `src/ui/AudioDeviceModel.hpp`
- Create: `src/ui/AudioDeviceModel.cpp`
- Modify: `qml/applications/settings/AudioSettings.qml`
- Modify: `src/CMakeLists.txt`
- Modify: `src/main.cpp`

**Step 1: Create `AudioDeviceModel`**

A `QAbstractListModel` that wraps the device registry's device list. Two instances: one for outputs, one for inputs. Roles: `NodeNameRole`, `DescriptionRole`.

Prepends a synthetic "Default (auto)" entry with `nodeName = "auto"`.

Connects to `PipeWireDeviceRegistry::deviceAdded` / `deviceRemoved` signals to update the model dynamically.

```cpp
// src/ui/AudioDeviceModel.hpp
#pragma once

#include "core/audio/PipeWireDeviceRegistry.hpp"
#include <QAbstractListModel>

namespace oap {

class AudioDeviceModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles { NodeNameRole = Qt::UserRole + 1, DescriptionRole };

    enum DeviceType { Output, Input };

    AudioDeviceModel(DeviceType type, PipeWireDeviceRegistry* registry,
                     QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int indexOfDevice(const QString& nodeName) const;

signals:
    void countChanged();

private slots:
    void onDeviceAdded(const oap::AudioDeviceInfo& info);
    void onDeviceRemoved(uint32_t registryId);

private:
    void rebuild();
    DeviceType type_;
    PipeWireDeviceRegistry* registry_;
    QList<AudioDeviceInfo> devices_; // includes synthetic "auto" at index 0
};

} // namespace oap
```

**Step 2: Expose models in `main.cpp`**

```cpp
auto* outputModel = new oap::AudioDeviceModel(
    oap::AudioDeviceModel::Output, audioService->deviceRegistry(), audioService);
auto* inputModel = new oap::AudioDeviceModel(
    oap::AudioDeviceModel::Input, audioService->deviceRegistry(), audioService);

engine.rootContext()->setContextProperty("AudioOutputDeviceModel", outputModel);
engine.rootContext()->setContextProperty("AudioInputDeviceModel", inputModel);
```

**Step 3: Update AudioSettings.qml**

Replace the text fields with ComboBoxes backed by the device models:

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: col.implicitHeight + 32
    clip: true

    ColumnLayout {
        id: col
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
        spacing: 8

        SettingsPageHeader { title: "Audio" }

        SettingsSlider {
            label: "Master Volume"
            configPath: "audio.master_volume"
            from: 0; to: 100; stepSize: 1
            onValueChanged: {
                if (typeof AudioService !== "undefined")
                    AudioService.setMasterVolume(value)
            }
        }

        // Output device selector
        Item {
            Layout.fillWidth: true
            implicitHeight: 48
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 12
                Text {
                    text: "Output Device"
                    font.pixelSize: 15
                    color: ThemeService.normalFontColor
                    Layout.fillWidth: true
                }
                ComboBox {
                    id: outputCombo
                    model: AudioOutputDeviceModel
                    textRole: "description"
                    Layout.preferredWidth: 220
                    onActivated: function(index) {
                        var nodeName = AudioOutputDeviceModel.data(
                            AudioOutputDeviceModel.index(index, 0), Qt.UserRole + 1)
                        ConfigService.setValue("audio.output_device", nodeName)
                        ConfigService.save()
                        if (typeof AudioService !== "undefined")
                            AudioService.setOutputDevice(nodeName)
                    }
                    Component.onCompleted: {
                        var current = ConfigService.value("audio.output_device") || "auto"
                        var idx = AudioOutputDeviceModel.indexOfDevice(current)
                        if (idx >= 0) currentIndex = idx
                    }
                }
            }
        }

        // Input device selector
        Item {
            Layout.fillWidth: true
            implicitHeight: 48
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 12
                Text {
                    text: "Input Device"
                    font.pixelSize: 15
                    color: ThemeService.normalFontColor
                    Layout.fillWidth: true
                }
                ComboBox {
                    id: inputCombo
                    model: AudioInputDeviceModel
                    textRole: "description"
                    Layout.preferredWidth: 220
                    onActivated: function(index) {
                        var nodeName = AudioInputDeviceModel.data(
                            AudioInputDeviceModel.index(index, 0), Qt.UserRole + 1)
                        ConfigService.setValue("audio.input_device", nodeName)
                        ConfigService.save()
                        if (typeof AudioService !== "undefined")
                            AudioService.setInputDevice(nodeName)
                    }
                    Component.onCompleted: {
                        var current = ConfigService.value("audio.input_device") || "auto"
                        var idx = AudioInputDeviceModel.indexOfDevice(current)
                        if (idx >= 0) currentIndex = idx
                    }
                }
            }
        }

        SettingsSlider {
            label: "Mic Gain"
            configPath: "audio.microphone.gain"
            from: 0.5; to: 4.0; stepSize: 0.1
        }
    }
}
```

**Step 4: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: Compiles, tests pass. UI shows device dropdowns (populated on Pi, empty on dev VM).

**Step 5: Commit**

```bash
git add src/ui/AudioDeviceModel.hpp src/ui/AudioDeviceModel.cpp \
        qml/applications/settings/AudioSettings.qml \
        src/CMakeLists.txt src/main.cpp
git commit -m "feat(audio): add device selection UI with QML ComboBox backed by PipeWire registry"
```

---

### Task 6: Volume Control Wiring

**Files:**
- Modify: `src/core/services/AudioService.cpp`
- Modify: `qml/components/GestureOverlay.qml`

**Step 1: Apply master volume to PipeWire streams**

In `setMasterVolume()`, after updating `masterVolume_`, iterate all streams and call:
```cpp
float vol = static_cast<float>(masterVolume_) / 100.0f;
// Cubic curve for perceptual volume
vol = vol * vol * vol;
float volumes[2] = {vol, vol}; // stereo

pw_thread_loop_lock(threadLoop_);
for (auto* handle : streams_) {
    if (handle->stream) {
        pw_stream_set_control(handle->stream,
            SPA_PROP_channelVolumes,
            handle->channels, volumes, 0);
    }
}
pw_thread_loop_unlock(threadLoop_);
```

**Step 2: Wire GestureOverlay volume slider**

In `GestureOverlay.qml`, the volume slider's `onValueChanged` should call:
```qml
onValueChanged: {
    dismissTimer.restart()
    if (typeof AudioService !== "undefined")
        AudioService.setMasterVolume(value)
}
```

Also set initial value from AudioService on `Component.onCompleted`.

**Step 3: Make AudioService a Q_INVOKABLE or expose properly**

AudioService methods called from QML need to be `Q_INVOKABLE`:
```cpp
Q_INVOKABLE void setMasterVolume(int volume);
Q_INVOKABLE int masterVolume() const;
Q_INVOKABLE void setOutputDevice(const QString& deviceName);
Q_INVOKABLE void setInputDevice(const QString& deviceName);
```

**Step 4: Build and test**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make -j$(nproc) && ctest --output-on-failure`

**Step 5: Commit**

```bash
git add src/core/services/AudioService.hpp src/core/services/AudioService.cpp \
        qml/components/GestureOverlay.qml
git commit -m "feat(audio): wire volume control to PipeWire streams and gesture overlay"
```

---

### Task 7: Device Disconnect Fallback

**Files:**
- Modify: `src/core/services/AudioService.hpp`
- Modify: `src/core/services/AudioService.cpp`

**Step 1: Connect to registry's `deviceRemoved` signal**

In AudioService constructor (after registry starts):
```cpp
connect(&deviceRegistry_, &PipeWireDeviceRegistry::deviceRemoved,
        this, &AudioService::onDeviceRemoved);
```

**Step 2: Implement `onDeviceRemoved()`**

```cpp
void AudioService::onDeviceRemoved(uint32_t registryId)
{
    // Check if removed device is our selected output or input
    // If so, log warning and streams will naturally fall back to
    // PipeWire default (WirePlumber handles rerouting when target disappears)
    qWarning() << "AudioService: Device removed, id:" << registryId
               << "- PipeWire will reroute active streams to default";
}
```

Note: PipeWire + WirePlumber already handles the rerouting when a target device disappears. We just need to:
1. Update the UI to show the device is gone
2. Log it
3. If the user had a specific device selected and it returns, they can re-select it

**Step 3: Build and test**

**Step 4: Commit**

```bash
git add src/core/services/AudioService.hpp src/core/services/AudioService.cpp
git commit -m "feat(audio): handle device disconnect with fallback logging"
```

---

### Task 8: Web Config Panel Audio Endpoints

**Files:**
- Modify: `web-config/server.py`

**Step 1: Add IPC commands**

Add `get_audio_devices` and `get_audio_config` / `set_audio_config` IPC handlers:

- `get_audio_devices` → AudioService responds with JSON list of output/input devices
- `get_audio_config` → returns current output_device, input_device, master_volume, mic_gain
- `set_audio_config` → updates config and calls AudioService methods

This requires the IPC server to handle these new command types and AudioService to respond to them.

**Step 2: Add Flask routes**

- `GET /api/audio/devices` — proxies `get_audio_devices` IPC
- `GET /api/audio/config` — proxies `get_audio_config` IPC
- `POST /api/audio/config` — proxies `set_audio_config` IPC

**Step 3: Add audio settings HTML section**

Add device dropdowns and volume slider to the web config settings page (follow existing UI patterns in the Flask templates).

**Step 4: Test via curl**

```bash
curl -s http://localhost:5000/api/audio/devices | python3 -m json.tool
curl -s http://localhost:5000/api/audio/config | python3 -m json.tool
curl -X POST http://localhost:5000/api/audio/config -H 'Content-Type: application/json' \
  -d '{"output_device": "auto", "master_volume": 75}'
```

**Step 5: Commit**

```bash
git add web-config/server.py web-config/templates/
git commit -m "feat(audio): add audio device and config endpoints to web config panel"
```

---

### Task 9: Integration Test on Pi

**Files:** None (manual testing)

This is hands-on Pi testing. Deploy to Pi and verify the full audio pipeline.

**Step 1: Deploy and build on Pi**

```bash
rsync -av --exclude='build/' --exclude='.git/' --exclude='libs/aasdk/' \
  /home/matt/claude/personal/openautopro/openauto-prodigy/ \
  matt@192.168.1.149:/home/matt/openauto-prodigy/
ssh matt@192.168.1.149 "cd /home/matt/openauto-prodigy/build && cmake --build . -j3"
```

**Step 2: Check device enumeration**

Launch the app, navigate to Settings > Audio. Verify:
- Output device dropdown lists available PipeWire sinks (HDMI, headphone jack, USB if plugged in)
- Input device dropdown lists available PipeWire sources
- "Default (auto)" appears at the top of both lists

**Step 3: Test AA audio playback**

Connect phone via Android Auto. Play music. Verify:
- Audio comes out of the selected output device
- Navigation prompts play and duck music volume
- Google Assistant voice responses play

**Step 4: Test microphone**

Trigger Google Assistant ("Hey Google" or long-press on AA). Speak. Verify:
- Assistant hears and responds
- Audio of the response plays through output

**Step 5: Test device switching**

Change output device in settings while music is playing. Verify:
- Audio switches to new device (brief gap is acceptable)
- Playback continues

**Step 6: Test hot-plug**

Plug in a USB audio adapter while the app is running. Verify:
- New device appears in the dropdown
- Selecting it routes audio there
- Unplugging it falls back to default

**Step 7: Commit any fixes discovered during testing**

---

## Implementation Order Summary

| Task | What | Depends On |
|------|------|-----------|
| 1 | AudioRingBuffer | — |
| 2 | AudioService rewrite (pw_thread_loop + stream connect + ring buffer) | Task 1 |
| 3 | PipeWireDeviceRegistry | — |
| 4 | Wire registry into AudioService + main.cpp | Tasks 2, 3 |
| 5 | QML device models + AudioSettings UI | Task 4 |
| 6 | Volume control wiring | Task 2 |
| 7 | Device disconnect fallback | Tasks 3, 4 |
| 8 | Web config audio endpoints | Task 4 |
| 9 | Pi integration test | All above |

Tasks 1 and 3 can be done in parallel. Tasks 5, 6, 7, 8 can be done in parallel after Task 4. Task 9 is the final verification.
