#include "AudioService.hpp"
#include <QDebug>

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

} // namespace oap
