#include "AudioService.hpp"
#include <QDebug>
#include <cstring>
#include <spa/param/props.h>

namespace oap {

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
        pw_core_disconnect(core_); core_ = nullptr;
        pw_context_destroy(context_); context_ = nullptr;
        pw_thread_loop_destroy(threadLoop_); threadLoop_ = nullptr;
        return;
    }

    qInfo() << "AudioService: Connected to PipeWire daemon";

    // Start device registry under PW lock (loop is now running)
    pw_thread_loop_lock(threadLoop_);
    deviceRegistry_.start(threadLoop_, core_);
    pw_thread_loop_unlock(threadLoop_);

    connect(&deviceRegistry_, &PipeWireDeviceRegistry::deviceRemoved,
            this, &AudioService::onDeviceRemoved);
}

AudioService::~AudioService()
{
    if (threadLoop_) {
        // All PW object teardown must happen under the PW lock while
        // the loop is still running.  Acquire PW lock FIRST, then mutex_
        // — same order as setMasterVolume() to prevent ABBA deadlock.
        pw_thread_loop_lock(threadLoop_);

        // Stop device registry (removes PW listener + proxy)
        deviceRegistry_.stop();

        // Disable capture callback before tearing down capture stream
        capture_.callbackActive.store(false, std::memory_order_release);

        // Destroy capture stream if active
        if (capture_.handle) {
            spa_hook_remove(&captureListener_);
            if (capture_.handle->stream)
                pw_stream_destroy(capture_.handle->stream);
            delete capture_.handle;
            capture_.handle = nullptr;
            capture_.callback = nullptr;
        }

        {
            QMutexLocker lock(&mutex_);
            for (auto* handle : streams_) {
                if (handle->stream) {
                    spa_hook_remove(&handle->listener);
                    pw_stream_destroy(handle->stream);
                }
                delete handle;
            }
            streams_.clear();
        }

        pw_thread_loop_unlock(threadLoop_);

        // Now safe to stop the loop (no PW objects left to callback into)
        pw_thread_loop_stop(threadLoop_);
    }

    if (core_)
        pw_core_disconnect(core_);
    if (context_)
        pw_context_destroy(context_);
    if (threadLoop_)
        pw_thread_loop_destroy(threadLoop_);

    pw_deinit();
}

// ---- Playback process callback (PipeWire RT thread) ----

void AudioService::onPlaybackProcess(void* userdata)
{
    auto* handle = static_cast<AudioStreamHandle*>(userdata);
    if (!handle || !handle->stream || !handle->ringBuffer)
        return;

    struct pw_buffer* buf = pw_stream_dequeue_buffer(handle->stream);
    if (!buf) return;

    struct spa_data& d = buf->buffer->datas[0];
    uint32_t maxSize = d.maxsize;

    uint32_t read = handle->ringBuffer->read(static_cast<uint8_t*>(d.data), maxSize);

    d.chunk->offset = 0;
    d.chunk->stride = handle->bytesPerFrame;
    d.chunk->size = read; // Only report actual audio bytes to PipeWire

    pw_stream_queue_buffer(handle->stream, buf);
}

// ---- Stream creation / destruction ----

AudioStreamHandle* AudioService::createStream(
    const QString& name, int priority,
    int sampleRate, int channels, const QString& targetDevice,
    int bufferMs)
{
    if (!isAvailable()) {
        qWarning() << "AudioService::createStream: PipeWire not available, returning nullptr";
        return nullptr;
    }

    auto* handle = new AudioStreamHandle();
    handle->name = name;
    handle->priority = priority;
    handle->sampleRate = sampleRate;
    handle->channels = channels;
    handle->bytesPerFrame = channels * 2; // 16-bit PCM
    handle->bufferMs = bufferMs;

    // Ring buffer: sized per-stream via bufferMs, rounded up to power of 2
    uint32_t rbSize = static_cast<uint32_t>(sampleRate * channels * 2 * (bufferMs / 1000.0f));
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

    // Set up process callback — userdata is the handle itself
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
        spa_hook_remove(&handle->listener);
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

int AudioService::writeAudio(AudioStreamHandle* handle, const uint8_t* data, int size)
{
    if (!handle || !handle->ringBuffer || !data || size <= 0)
        return -1;

    return static_cast<int>(handle->ringBuffer->write(data, static_cast<uint32_t>(size)));
}

// ---- Volume & Audio Focus ----

void AudioService::setMasterVolume(int volume)
{
    // Lock ordering: PW lock first, then mutex_ (same as destructor)
    if (!threadLoop_) {
        QMutexLocker lock(&mutex_);
        masterVolume_ = qBound(0, volume, 100);
        emit masterVolumeChanged();
        return;
    }

    pw_thread_loop_lock(threadLoop_);
    {
        QMutexLocker lock(&mutex_);
        masterVolume_ = qBound(0, volume, 100);

        // Cubic curve for perceptual volume scaling
        float vol = static_cast<float>(masterVolume_) / 100.0f;
        vol = vol * vol * vol;

        for (auto* handle : streams_) {
            if (handle->stream) {
                float volumes[] = {vol, vol}; // up to stereo
                pw_stream_set_control(handle->stream,
                    SPA_PROP_channelVolumes,
                    static_cast<uint32_t>(handle->channels), volumes, 0);
            }
        }
    }
    pw_thread_loop_unlock(threadLoop_);
    emit masterVolumeChanged();
}

int AudioService::masterVolume() const
{
    QMutexLocker lock(&mutex_);
    return masterVolume_;
}

void AudioService::requestAudioFocus(AudioStreamHandle* handle, AudioFocusType type)
{
    if (!handle) return;

    QMutexLocker lock(&mutex_);
    handle->hasFocus = true;
    handle->focusType = type;
    applyDucking();
}

void AudioService::releaseAudioFocus(AudioStreamHandle* handle)
{
    if (!handle) return;

    QMutexLocker lock(&mutex_);
    handle->hasFocus = false;
    applyDucking();
}

void AudioService::applyDucking()
{
    // Find the highest-priority stream with focus
    AudioStreamHandle* dominant = nullptr;
    for (auto* s : streams_) {
        if (s->hasFocus) {
            if (!dominant || s->priority > dominant->priority)
                dominant = s;
        }
    }

    if (!dominant) {
        // No stream has focus — restore all volumes
        for (auto* s : streams_)
            s->volume = s->baseVolume;
        return;
    }

    for (auto* s : streams_) {
        if (s == dominant) {
            s->volume = s->baseVolume;
        } else if (dominant->focusType == AudioFocusType::GainTransientMayDuck) {
            // Duck lower-priority streams to 20%
            s->volume = s->baseVolume * 0.2f;
        } else {
            // Gain or GainTransient — mute lower-priority streams
            s->volume = 0.0f;
        }
    }
}

// ---- Device Selection ----

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

// ---- Capture (microphone input) ----

void AudioService::onCaptureProcess(void* userdata)
{
    auto* self = static_cast<AudioService*>(userdata);

    if (!self->capture_.handle || !self->capture_.handle->stream)
        return;

    struct pw_buffer* buf = pw_stream_dequeue_buffer(self->capture_.handle->stream);
    if (!buf) return;

    struct spa_data& d = buf->buffer->datas[0];
    // Use atomic guard to safely check callback — avoids race with
    // closeCaptureStream() clearing the std::function on another thread.
    if (d.data && d.chunk->size > 0 &&
        self->capture_.callbackActive.load(std::memory_order_acquire)) {
        auto* ptr = static_cast<const uint8_t*>(d.data) + d.chunk->offset;
        int size = static_cast<int>(d.chunk->size);
        self->capture_.callback(ptr, size);
    }

    pw_stream_queue_buffer(self->capture_.handle->stream, buf);
}

AudioStreamHandle* AudioService::openCaptureStream(const QString& name,
                                                     int sampleRate, int channels, int bitDepth)
{
    if (!isAvailable()) {
        qWarning() << "AudioService::openCaptureStream: PipeWire not available";
        return nullptr;
    }

    // Only one capture stream at a time
    if (capture_.handle) {
        qWarning() << "AudioService::openCaptureStream: capture already open, closing previous";
        closeCaptureStream(capture_.handle);
    }

    auto* handle = new AudioStreamHandle();
    handle->name = name;
    handle->priority = 0;

    pw_thread_loop_lock(threadLoop_);

    auto props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Communication",
        PW_KEY_NODE_NAME, name.toUtf8().constData(),
        PW_KEY_APP_NAME, "OpenAuto Prodigy",
        nullptr);

    // Target specific input device if not "auto"
    {
        QMutexLocker lock(&mutex_);
        if (inputDevice_ != "auto" && !inputDevice_.isEmpty()) {
            pw_properties_set(props, PW_KEY_TARGET_OBJECT, inputDevice_.toUtf8().constData());
        }
    }

    handle->stream = pw_stream_new(core_, name.toUtf8().constData(), props);
    if (!handle->stream) {
        qWarning() << "AudioService: Failed to create PipeWire capture stream:" << name;
        pw_thread_loop_unlock(threadLoop_);
        delete handle;
        return nullptr;
    }

    // Set up process callback
    capture_.handle = handle;
    capture_.events = {};
    capture_.events.version = PW_VERSION_STREAM_EVENTS;
    capture_.events.process = &AudioService::onCaptureProcess;

    pw_stream_add_listener(handle->stream, &captureListener_,
                           &capture_.events, this);

    // Build audio format params
    uint8_t paramBuf[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(paramBuf, sizeof(paramBuf));

    struct spa_audio_info_raw rawInfo{};
    rawInfo.format = (bitDepth == 32) ? SPA_AUDIO_FORMAT_S32_LE : SPA_AUDIO_FORMAT_S16_LE;
    rawInfo.rate = static_cast<uint32_t>(sampleRate);
    rawInfo.channels = static_cast<uint32_t>(channels);

    const struct spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    int ret = pw_stream_connect(handle->stream,
        PW_DIRECTION_INPUT,
        PW_ID_ANY,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
        params, 1);

    if (ret < 0) {
        qWarning() << "AudioService: Failed to connect capture stream:" << name << "error:" << ret;
        spa_hook_remove(&captureListener_);
        pw_stream_destroy(handle->stream);
        pw_thread_loop_unlock(threadLoop_);
        capture_.handle = nullptr;
        delete handle;
        return nullptr;
    }

    pw_thread_loop_unlock(threadLoop_);

    qInfo() << "AudioService: Opened capture stream" << name
            << sampleRate << "Hz" << channels << "ch" << bitDepth << "bit";
    return handle;
}

void AudioService::closeCaptureStream(AudioStreamHandle* handle)
{
    if (!handle) return;

    if (capture_.handle == handle) {
        // Disable callback atomically first — stops RT thread from invoking it
        capture_.callbackActive.store(false, std::memory_order_release);
        capture_.callback = nullptr;

        if (threadLoop_) {
            pw_thread_loop_lock(threadLoop_);
            // Remove the spa_hook BEFORE destroying the stream — otherwise the
            // hook's list pointers dangle and the next openCaptureStream triggers
            // a use-after-free when pw_stream_add_listener reuses captureListener_.
            spa_hook_remove(&captureListener_);
            if (handle->stream)
                pw_stream_destroy(handle->stream);
            pw_thread_loop_unlock(threadLoop_);
        } else {
            if (handle->stream)
                pw_stream_destroy(handle->stream);
        }

        capture_.handle = nullptr;
    }

    qInfo() << "AudioService: Closed capture stream" << handle->name;
    delete handle;
}

void AudioService::setCaptureCallback(AudioStreamHandle* handle, CaptureCallback cb)
{
    if (handle && capture_.handle == handle) {
        capture_.callback = std::move(cb);
        capture_.callbackActive.store(capture_.callback != nullptr, std::memory_order_release);
    }
}

// ---- Device disconnect handling ----

void AudioService::onDeviceRemoved(uint32_t registryId)
{
    Q_UNUSED(registryId);
    // PipeWire/WirePlumber handles the actual rerouting when a target device
    // disappears. Active streams will automatically fall back to the default
    // sink/source. We just log it for diagnostics.
    qWarning() << "AudioService: Audio device removed (registry id:" << registryId
               << ") — PipeWire will reroute active streams to default";
}

} // namespace oap
