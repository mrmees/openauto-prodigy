#include "AudioService.hpp"
#include <QDebug>
#include <cstring>
#include <spa/param/audio/format-utils.h>

namespace oap {

AudioService::AudioService(QObject* parent)
    : QObject(parent)
{
    pw_init(nullptr, nullptr);

    // Try to connect to PipeWire daemon.
    // This may fail on dev VM where PipeWire is not running as a session daemon.
    mainLoop_ = pw_main_loop_new(nullptr);
    if (!mainLoop_) {
        qWarning() << "AudioService: Failed to create PipeWire main loop";
        return;
    }

    context_ = pw_context_new(pw_main_loop_get_loop(mainLoop_), nullptr, 0);
    if (!context_) {
        qWarning() << "AudioService: Failed to create PipeWire context";
        pw_main_loop_destroy(mainLoop_);
        mainLoop_ = nullptr;
        return;
    }

    core_ = pw_context_connect(context_, nullptr, 0);
    if (!core_) {
        qWarning() << "AudioService: Failed to connect to PipeWire daemon"
                    << " — audio will be unavailable";
        pw_context_destroy(context_);
        context_ = nullptr;
        pw_main_loop_destroy(mainLoop_);
        mainLoop_ = nullptr;
        return;
    }

    qInfo() << "AudioService: Connected to PipeWire daemon";
}

AudioService::~AudioService()
{
    // Destroy all remaining streams
    QMutexLocker lock(&mutex_);
    for (auto* handle : streams_) {
        if (handle->stream)
            pw_stream_destroy(handle->stream);
        delete handle;
    }
    streams_.clear();
    lock.unlock();

    if (core_)
        pw_core_disconnect(core_);
    if (context_)
        pw_context_destroy(context_);
    if (mainLoop_)
        pw_main_loop_destroy(mainLoop_);

    pw_deinit();
}

AudioStreamHandle* AudioService::createStream(const QString& name, int priority)
{
    if (!isAvailable()) {
        qWarning() << "AudioService::createStream: PipeWire not available, returning nullptr";
        return nullptr;
    }

    auto* handle = new AudioStreamHandle();
    handle->name = name;
    handle->priority = priority;

    // Create PipeWire stream
    auto props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_NODE_NAME, name.toUtf8().constData(),
        PW_KEY_APP_NAME, "OpenAuto Prodigy",
        nullptr);

    handle->stream = pw_stream_new(core_, name.toUtf8().constData(), props);
    if (!handle->stream) {
        qWarning() << "AudioService: Failed to create PipeWire stream:" << name;
        delete handle;
        return nullptr;
    }

    QMutexLocker lock(&mutex_);
    streams_.append(handle);

    qInfo() << "AudioService: Created stream" << name << "priority:" << priority;
    return handle;
}

void AudioService::destroyStream(AudioStreamHandle* handle)
{
    if (!handle) return;

    QMutexLocker lock(&mutex_);
    streams_.removeOne(handle);
    lock.unlock();

    if (handle->stream)
        pw_stream_destroy(handle->stream);

    qInfo() << "AudioService: Destroyed stream" << handle->name;
    delete handle;
}

int AudioService::writeAudio(AudioStreamHandle* handle, const uint8_t* data, int size)
{
    if (!handle || !handle->stream || !data || size <= 0)
        return -1;

    // Get a buffer from PipeWire stream
    struct pw_buffer* buf = pw_stream_dequeue_buffer(handle->stream);
    if (!buf) return 0;  // No buffer available, not an error

    struct spa_data& d = buf->buffer->datas[0];
    int written = qMin(size, static_cast<int>(d.maxsize));
    memcpy(d.data, data, written);
    d.chunk->offset = 0;
    d.chunk->stride = 4;  // Assuming stereo 16-bit (2 channels * 2 bytes)
    d.chunk->size = written;

    pw_stream_queue_buffer(handle->stream, buf);
    return written;
}

void AudioService::setMasterVolume(int volume)
{
    QMutexLocker lock(&mutex_);
    masterVolume_ = qBound(0, volume, 100);
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

// ---- Capture (microphone input) ----

void AudioService::onCaptureProcess(void* userdata)
{
    auto* self = static_cast<AudioService*>(userdata);

    if (!self->capture_.handle || !self->capture_.handle->stream)
        return;

    struct pw_buffer* buf = pw_stream_dequeue_buffer(self->capture_.handle->stream);
    if (!buf) return;

    struct spa_data& d = buf->buffer->datas[0];
    if (d.data && d.chunk->size > 0 && self->capture_.callback) {
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

    auto props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Communication",
        PW_KEY_NODE_NAME, name.toUtf8().constData(),
        PW_KEY_APP_NAME, "OpenAuto Prodigy",
        nullptr);

    handle->stream = pw_stream_new(core_, name.toUtf8().constData(), props);
    if (!handle->stream) {
        qWarning() << "AudioService: Failed to create PipeWire capture stream:" << name;
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
        pw_stream_destroy(handle->stream);
        capture_.handle = nullptr;
        delete handle;
        return nullptr;
    }

    qInfo() << "AudioService: Opened capture stream" << name
            << sampleRate << "Hz" << channels << "ch" << bitDepth << "bit";
    return handle;
}

void AudioService::closeCaptureStream(AudioStreamHandle* handle)
{
    if (!handle) return;

    if (capture_.handle == handle) {
        capture_.callback = nullptr;
        capture_.handle = nullptr;
    }

    if (handle->stream)
        pw_stream_destroy(handle->stream);

    qInfo() << "AudioService: Closed capture stream" << handle->name;
    delete handle;
}

void AudioService::setCaptureCallback(AudioStreamHandle* handle, CaptureCallback cb)
{
    if (handle && capture_.handle == handle) {
        capture_.callback = std::move(cb);
    }
}

} // namespace oap
