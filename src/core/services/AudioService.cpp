#include "AudioService.hpp"
#include "../Logging.hpp"
#include "core/audio/EqualizerEngine.hpp"
#include "core/audio/FocusGain.hpp"
#include <QCoreApplication>
#include <algorithm>
#include <cstring>
#include <spa/param/props.h>
#include <pipewire/version.h>

// pw_stream_set_rate() requires PipeWire >= 1.4.0 (Pi has 1.4.2, dev VM has 1.0.5)
#if !defined(PW_CHECK_VERSION) || !PW_CHECK_VERSION(1, 4, 0)
static inline int pw_stream_set_rate(struct pw_stream*, double) { return 0; }
#endif

namespace oap {

AudioService::AudioService(QObject* parent)
    : QObject(parent)
{
    pw_init(nullptr, nullptr);

    threadLoop_ = pw_thread_loop_new("openauto-audio", nullptr);
    if (!threadLoop_) {
        qCWarning(lcAudio) << "AudioService: Failed to create PipeWire thread loop";
        return;
    }

    pw_thread_loop_lock(threadLoop_);

    context_ = pw_context_new(pw_thread_loop_get_loop(threadLoop_), nullptr, 0);
    if (!context_) {
        qCWarning(lcAudio) << "AudioService: Failed to create PipeWire context";
        pw_thread_loop_unlock(threadLoop_);
        pw_thread_loop_destroy(threadLoop_);
        threadLoop_ = nullptr;
        return;
    }

    core_ = pw_context_connect(context_, nullptr, 0);
    if (!core_) {
        qCWarning(lcAudio) << "AudioService: Failed to connect to PipeWire daemon"
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
        qCWarning(lcAudio) << "AudioService: Failed to start PipeWire thread loop";
        pw_core_disconnect(core_); core_ = nullptr;
        pw_context_destroy(context_); context_ = nullptr;
        pw_thread_loop_destroy(threadLoop_); threadLoop_ = nullptr;
        return;
    }

    qCInfo(lcAudio) << "AudioService: Connected to PipeWire daemon";

    // Start device registry under PW lock (loop is now running)
    pw_thread_loop_lock(threadLoop_);
    deviceRegistry_.start(threadLoop_, core_);
    pw_thread_loop_unlock(threadLoop_);

    connect(&deviceRegistry_, &PipeWireDeviceRegistry::deviceRemoved,
            this, &AudioService::onDeviceRemoved);

    // Adaptive buffer growth timer — polls underrun counters every 2 seconds
    connect(&adaptiveTimer_, &QTimer::timeout, this, &AudioService::checkAdaptiveBuffers);
    adaptiveTimer_.start(2000);
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

        {
            QMutexLocker lock(&mutex_);

            // Tear down capture streams — each carries its own listener.
            for (auto* handle : captures_) {
                // Disable the RT callback guard before destroying the stream.
                handle->captureCallbackActive.store(false, std::memory_order_release);
                if (handle->stream) {
                    spa_hook_remove(&handle->listener);
                    pw_stream_destroy(handle->stream);
                }
                delete handle;
            }
            captures_.clear();

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
    int stride = handle->bytesPerFrame;

    // Determine how many frames to output: clamp to buf->requested when nonzero,
    // matching PipeWire's audio-src-ring.c example. The resampler sets requested
    // to the exact frame count it needs; maxsize is just the buffer capacity.
    uint32_t n_frames = d.maxsize / stride;
    if (buf->requested > 0 && buf->requested < n_frames)
        n_frames = buf->requested;
    uint32_t wantBytes = n_frames * stride;

    uint32_t bytesRead = handle->ringBuffer->read(static_cast<uint8_t*>(d.data), wantBytes);

    // Track complete underruns for adaptive buffer growth
    if (bytesRead == 0)
        handle->underrunCount.fetch_add(1, std::memory_order_relaxed);

    // EQ processing (RT-safe, in-place) — runs before silence fill
    if (handle->eqEngine && bytesRead > 0) {
        int frames = static_cast<int>(bytesRead / handle->bytesPerFrame);
        handle->eqEngine->process(
            reinterpret_cast<int16_t*>(static_cast<uint8_t*>(d.data)),
            frames);
    }

    // Focus gain (duck/mute from applyDucking) — ramped to avoid clicks
    if (bytesRead > 0) {
        const float target = handle->targetGain.load(std::memory_order_relaxed);
        if (target != 1.0f || handle->rtCurrentGain != 1.0f) {
            int frames = static_cast<int>(bytesRead / handle->bytesPerFrame);
            handle->rtCurrentGain = applyFocusGain(
                reinterpret_cast<int16_t*>(static_cast<uint8_t*>(d.data)),
                frames, handle->channels, handle->rtCurrentGain, target);
        }
    }

    // Silence-fill any gap — PipeWire graph timing is fixed by quantum/rate,
    // so we must always output a full period to avoid tempo wobble.
    if (bytesRead < wantBytes)
        std::memset(static_cast<uint8_t*>(d.data) + bytesRead, 0, wantBytes - bytesRead);

    d.chunk->offset = 0;
    d.chunk->stride = stride;
    d.chunk->size = wantBytes;
    buf->size = n_frames;

    // --- Adaptive rate matching (clock drift compensation) ---
    // The phone's audio clock and PipeWire's graph clock drift independently.
    // We steer PipeWire's built-in adaptive resampler via pw_stream_set_rate()
    // to keep the ring buffer fill level near the target (50% of capacity).
    // Opt-out per handle (e.g. BT A2DP loopback tap owns its own clocking).
    if (!handle->disableRateMatching) {
        uint32_t avail = handle->ringBuffer->available();

        // Track activity and update EMA every callback for proper smoothing
        if (bytesRead > 0)
            handle->activeCallbacks++;

        float fillNorm = static_cast<float>(avail) /
                         static_cast<float>(handle->ringBuffer->capacity());
        constexpr float alpha = 0.02f; // ~50 callback time constant (~1s)
        handle->filteredFill += alpha * (fillNorm - handle->filteredFill);

        // Apply rate correction every ~16 callbacks (~340ms)
        handle->rateCtlCount++;
        if (handle->rateCtlCount >= 16) {
            bool active = handle->activeCallbacks > 0;
            handle->activeCallbacks = 0;
            handle->rateCtlCount = 0;

            if (active) {
                // Target 25% fill — with 500ms buffer this is ~125ms of audio.
                // The phone naturally delivers at roughly playback rate, so the
                // buffer settles around 20%. Targeting too high (75%) saturates
                // the integrator and prevents the controller from responding to
                // actual variations.
                float error = handle->filteredFill - 0.25f;

                // PI controller — gentle gains to avoid oscillation
                constexpr float kP = 0.001f;
                constexpr float kI = 0.00005f;

                handle->rateIntegral += error;
                // Anti-windup
                if (handle->rateIntegral > 10.0f) handle->rateIntegral = 10.0f;
                if (handle->rateIntegral < -10.0f) handle->rateIntegral = -10.0f;

                float correction = (kP * error) + (kI * handle->rateIntegral);

                // Clamp to ±5000 ppm (±0.5%)
                if (correction > 0.005f) correction = 0.005f;
                if (correction < -0.005f) correction = -0.005f;

                double newRate = 1.0 + static_cast<double>(correction);
                pw_stream_set_rate(handle->stream, newRate);

                // Periodic diagnostic (every ~10s)
                handle->diagCount++;
                if (handle->diagCount % 30 == 0) {
                    uint32_t drops = handle->ringBuffer->resetDropCount();
                    fprintf(stderr, "[AudioRate %s] fill=%.1f%% err=%.4f integ=%.2f corr=%.6f avail=%u/%u drops=%u\n",
                            handle->name.toUtf8().constData(),
                            handle->filteredFill * 100.0f, error,
                            handle->rateIntegral, correction,
                            avail, handle->ringBuffer->capacity(), drops);
                }
            } else {
                handle->filteredFill = 0.25f;
                handle->rateIntegral = 0.0f;
                pw_stream_set_rate(handle->stream, 1.0);
            }
        }
    }

    pw_stream_queue_buffer(handle->stream, buf);
}

// ---- Playback state-change callback (PipeWire thread) ----

void AudioService::onPlaybackStateChanged(void* userdata, enum pw_stream_state old,
                                          enum pw_stream_state state, const char* error)
{
    Q_UNUSED(old);
    Q_UNUSED(error);
    auto* handle = static_cast<AudioStreamHandle*>(userdata);
    if (!handle)
        return;

    // onStreamError / errorContext are set before connect and never mutated, so
    // these reads are safe on the PW thread. Never run the hook here — marshal
    // it to the Qt main thread. Dispatch against errorContext when supplied so
    // Qt auto-cancels the queued call if the consumer is destroyed first;
    // otherwise fall back to qApp.
    if (state == PW_STREAM_STATE_ERROR && handle->onStreamError) {
        QObject* ctx = handle->errorContext ? handle->errorContext
                                            : static_cast<QObject*>(qApp);
        QMetaObject::invokeMethod(ctx, handle->onStreamError, Qt::QueuedConnection);
    }
}

// ---- Stream creation / destruction ----

AudioStreamHandle* AudioService::createStream(
    const QString& name, int priority,
    int sampleRate, int channels, const QString& targetDevice,
    int bufferMs)
{
    // Legacy virtual — one code path through createStreamWithOptions().
    PlaybackStreamOptions opts;
    opts.name = name;
    opts.priority = priority;
    opts.sampleRate = sampleRate;
    opts.channels = channels;
    opts.targetDevice = targetDevice;
    opts.bufferMs = bufferMs;
    return createStreamWithOptions(opts);
}

AudioStreamHandle* AudioService::createStreamWithOptions(const PlaybackStreamOptions& opts)
{
    if (!isAvailable()) {
        qCWarning(lcAudio) << "AudioService::createStreamWithOptions: PipeWire not available, returning nullptr";
        return nullptr;
    }

    // App playback streams are mono/stereo. Clamp into [1,2] up front: the EQ
    // engine already clamps to 1-2, the bytesPerFrame math assumes it, and the
    // volume control writes a 2-slot array — a >2-channel request would desync
    // the format from those assumptions. Nothing currently requests >2, so this
    // is defensive.
    int channels = opts.channels;
    if (channels < 1 || channels > 2) {
        qCWarning(lcAudio) << "AudioService::createStreamWithOptions: channels" << channels
                           << "out of [1,2] for" << opts.name << "- clamping";
        channels = qBound(1, channels, 2);
    }

    auto* handle = new AudioStreamHandle();
    handle->name = opts.name;
    handle->priority = opts.priority;
    handle->sampleRate = opts.sampleRate;
    handle->channels = channels;
    handle->bytesPerFrame = channels * 2; // 16-bit PCM
    handle->eqEngine = opts.eqEngine;                     // attached BEFORE connect
    handle->disableRateMatching = opts.disableRateMatching;
    handle->onStreamError = opts.onStreamError;
    handle->errorContext = opts.errorContext;

    // Ring buffer: sized per-stream via bufferMs, minimum 500ms for burst absorption
    // AA sends audio in large protobuf bursts over TCP, not sample-by-sample.
    int bufferMs = opts.bufferMs;
    if (bufferMs < 500) bufferMs = 500;
    handle->bufferMs = bufferMs;
    uint32_t rbSize = static_cast<uint32_t>(opts.sampleRate * channels * 2 * (bufferMs / 1000.0f));
    uint32_t pow2 = 1;
    while (pow2 < rbSize) pow2 <<= 1;
    handle->ringBuffer = std::make_unique<AudioRingBuffer>(pow2);
    qCDebug(lcAudio) << "AudioService: Ring buffer for" << opts.name << ":" << pow2 << "bytes (" << bufferMs << "ms)";

    // Determine PipeWire role based on stream name
    const char* role = "Music";
    if (opts.name.contains("Navigation") || opts.name.contains("Speech"))
        role = "Communication";
    else if (opts.name.contains("System"))
        role = "Notification";

    pw_thread_loop_lock(threadLoop_);

    auto props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, role,
        PW_KEY_NODE_NAME, opts.name.toUtf8().constData(),
        PW_KEY_APP_NAME, "OpenAuto Prodigy",
        nullptr);

    // Target specific device if not "auto"
    QString device = opts.targetDevice == "auto" ? outputDevice_ : opts.targetDevice;
    if (device != "auto" && !device.isEmpty()) {
        pw_properties_set(props, PW_KEY_TARGET_OBJECT, device.toUtf8().constData());
    }

    handle->stream = pw_stream_new(core_, opts.name.toUtf8().constData(), props);
    if (!handle->stream) {
        qCWarning(lcAudio) << "AudioService: Failed to create PipeWire stream:" << opts.name;
        pw_thread_loop_unlock(threadLoop_);
        delete handle;
        return nullptr;
    }

    // Set up process + state-change callbacks — userdata is the handle itself
    handle->events = {};
    handle->events.version = PW_VERSION_STREAM_EVENTS;
    handle->events.process = &AudioService::onPlaybackProcess;
    handle->events.state_changed = &AudioService::onPlaybackStateChanged;
    pw_stream_add_listener(handle->stream, &handle->listener, &handle->events, handle);

    // Build format params
    uint8_t paramBuf[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(paramBuf, sizeof(paramBuf));

    struct spa_audio_info_raw rawInfo{};
    rawInfo.format = SPA_AUDIO_FORMAT_S16_LE;
    rawInfo.rate = static_cast<uint32_t>(opts.sampleRate);
    rawInfo.channels = static_cast<uint32_t>(channels);

    const struct spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    pw_stream_flags connectFlags = static_cast<pw_stream_flags>(
        PW_STREAM_FLAG_AUTOCONNECT |
        PW_STREAM_FLAG_MAP_BUFFERS |
        PW_STREAM_FLAG_RT_PROCESS);
    if (opts.startInactive)
        connectFlags = static_cast<pw_stream_flags>(connectFlags | PW_STREAM_FLAG_INACTIVE);

    int ret = pw_stream_connect(handle->stream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        connectFlags,
        params, 1);

    if (ret < 0) {
        qCWarning(lcAudio) << "AudioService: Failed to connect stream:" << opts.name << "error:" << ret;
        spa_hook_remove(&handle->listener);
        pw_stream_destroy(handle->stream);
        pw_thread_loop_unlock(threadLoop_);
        delete handle;
        return nullptr;
    }

    // Volume-at-creation (design §4.2): apply the current master volume now, so a
    // stream born at reduced volume never plays a full-volume first period.
    // Read masterVolume_ under mutex_ INSIDE the PW lock — same order as
    // setMasterVolume() to prevent ABBA deadlock.
    {
        QMutexLocker lock(&mutex_);
        applyVolumeToStream(handle, cubicVolume(masterVolume_));
    }

    pw_thread_loop_unlock(threadLoop_);

    QMutexLocker lock(&mutex_);
    streams_.append(handle);

    qCInfo(lcAudio) << "AudioService: Created stream" << opts.name
            << opts.sampleRate << "Hz" << opts.channels << "ch"
            << "priority:" << opts.priority;
    return handle;
}

void AudioService::destroyStream(AudioStreamHandle* handle)
{
    if (!handle) return;

    QMutexLocker lock(&mutex_);
    streams_.removeOne(handle);
    // The destroyed stream may have been the dominant focus holder (e.g. AA
    // teardown mid-playback) — recompute so survivors aren't muted forever.
    applyDucking();
    lock.unlock();

    if (handle->stream && threadLoop_) {
        pw_thread_loop_lock(threadLoop_);
        spa_hook_remove(&handle->listener);
        pw_stream_destroy(handle->stream);
        pw_thread_loop_unlock(threadLoop_);
    }

    qCInfo(lcAudio) << "AudioService: Destroyed stream" << handle->name;
    delete handle;
}

int AudioService::writeAudio(AudioStreamHandle* handle, const uint8_t* data, int size)
{
    if (!handle || !handle->ringBuffer || !data || size <= 0)
        return -1;

    return static_cast<int>(handle->ringBuffer->write(data, static_cast<uint32_t>(size)));
}

// ---- Volume & Audio Focus ----

void AudioService::applyVolumeToStream(AudioStreamHandle* handle, float vol)
{
    if (!handle || !handle->stream) return;
    // App streams are mono/stereo; a sized 2-slot array bounds the control write.
    // A >2-channel volume would need a larger array — cap the count so we never
    // hand pw_stream_set_control a length past the array (design §4.2 / OOB fix).
    const uint32_t nch = static_cast<uint32_t>(std::min(handle->channels, 2));
    float volumes[2] = {vol, vol};
    int r = pw_stream_set_control(handle->stream, SPA_PROP_channelVolumes,
                                  nch, volumes, 0);
    if (r < 0)
        qCWarning(lcAudio) << "AudioService: set channelVolumes failed for"
                           << handle->name << "err" << r;
}

float AudioService::cubicVolume(int masterVolume0to100)
{
    // Perceptual cubic curve: (v/100)^3. Clamped so the curve is total.
    float vol = static_cast<float>(qBound(0, masterVolume0to100, 100)) / 100.0f;
    return vol * vol * vol;
}

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
        float vol = cubicVolume(masterVolume_);

        for (auto* handle : streams_)
            applyVolumeToStream(handle, vol);
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
    handle->focusSequence = ++focusSeqCounter_;  // stamp recency for tie-breaks
    applyDucking();
}

void AudioService::releaseAudioFocus(AudioStreamHandle* handle)
{
    if (!handle) return;

    QMutexLocker lock(&mutex_);
    handle->hasFocus = false;
    applyDucking();
}

AudioStreamHandle* AudioService::selectDominant(const QList<AudioStreamHandle*>& streams)
{
    AudioStreamHandle* dominant = nullptr;
    for (auto* s : streams) {
        if (!s->hasFocus) continue;
        if (!dominant
            || s->priority > dominant->priority
            || (s->priority == dominant->priority
                && s->focusSequence > dominant->focusSequence))
            dominant = s;
    }
    return dominant;
}

void AudioService::applyDucking()
{
    // Dominant = highest-priority focus holder; ties broken by focus recency.
    AudioStreamHandle* dominant = selectDominant(streams_);

    if (!dominant) {
        // No stream has focus — restore all volumes
        for (auto* s : streams_)
            s->targetGain.store(s->baseVolume, std::memory_order_relaxed);
        return;
    }

    for (auto* s : streams_) {
        if (s == dominant) {
            s->targetGain.store(s->baseVolume, std::memory_order_relaxed);
        } else if (dominant->focusType == AudioFocusType::GainTransientMayDuck) {
            // Duck lower-priority streams to 20%
            s->targetGain.store(s->baseVolume * 0.2f, std::memory_order_relaxed);
        } else {
            // Gain or GainTransient — mute lower-priority streams
            s->targetGain.store(0.0f, std::memory_order_relaxed);
        }
    }
}

// ---- Device Selection ----

void AudioService::setOutputDevice(const QString& deviceName)
{
    QMutexLocker lock(&mutex_);
    outputDevice_ = deviceName.isEmpty() ? "auto" : deviceName;
    qCDebug(lcAudio) << "AudioService: Output device set to" << outputDevice_;
}

void AudioService::setInputDevice(const QString& deviceName)
{
    QMutexLocker lock(&mutex_);
    inputDevice_ = deviceName.isEmpty() ? "auto" : deviceName;
    qCDebug(lcAudio) << "AudioService: Input device set to" << inputDevice_;
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

// ---- Capture (microphone / loopback input) ----

void AudioService::onCaptureProcess(void* userdata)
{
    // userdata is the capture handle (like playback), not the service.
    auto* handle = static_cast<AudioStreamHandle*>(userdata);
    if (!handle || !handle->stream)
        return;

    struct pw_buffer* buf = pw_stream_dequeue_buffer(handle->stream);
    if (!buf) return;

    struct spa_data& d = buf->buffer->datas[0];
    // Atomic guard: a legacy setCaptureCallback / close may be mutating the
    // std::function on the Qt thread. Only touch it when published active.
    if (d.data && d.chunk->size > 0 &&
        handle->captureCallbackActive.load(std::memory_order_acquire) &&
        handle->captureCallback) {
        auto* ptr = static_cast<const uint8_t*>(d.data) + d.chunk->offset;
        int size = static_cast<int>(d.chunk->size);
        handle->captureCallback(ptr, size);
    }

    pw_stream_queue_buffer(handle->stream, buf);
}

AudioStreamHandle* AudioService::openCaptureStream(const QString& name,
                                                     int sampleRate, int channels, int bitDepth)
{
    // Legacy virtual — autoconnecting, no pre-connect callback (installed later
    // via setCaptureCallback).
    CaptureStreamOptions opts;
    opts.name = name;
    opts.sampleRate = sampleRate;
    opts.channels = channels;
    opts.bitDepth = bitDepth;
    opts.autoconnect = true;
    return openCaptureStreamWithOptions(opts);
}

AudioStreamHandle* AudioService::openCaptureStreamWithOptions(const CaptureStreamOptions& opts)
{
    if (!isAvailable()) {
        qCWarning(lcAudio) << "AudioService::openCaptureStreamWithOptions: PipeWire not available";
        return nullptr;
    }

    auto* handle = new AudioStreamHandle();
    handle->name = opts.name;
    handle->priority = 0;
    handle->isCapture = true;
    handle->sampleRate = opts.sampleRate;
    handle->channels = opts.channels;
    handle->bytesPerFrame = opts.channels * (opts.bitDepth == 32 ? 4 : 2);
    // A pre-connect callback is immutable — installed before connect and
    // published for the RT thread here (no daemon has run any callback yet). The
    // immutable flag is set ONLY when such a callback is actually present, so a
    // capture handle opened WITHOUT one stays a mutable legacy handle.
    handle->captureCallback = opts.callback;
    handle->captureCallbackImmutable = static_cast<bool>(opts.callback);
    handle->captureCallbackActive.store(static_cast<bool>(opts.callback),
                                        std::memory_order_release);

    pw_thread_loop_lock(threadLoop_);

    auto props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Communication",
        PW_KEY_NODE_NAME, opts.name.toUtf8().constData(),
        PW_KEY_APP_NAME, "OpenAuto Prodigy",
        nullptr);

    // Target specific input device only when autoconnecting.
    if (opts.autoconnect) {
        QMutexLocker lock(&mutex_);
        if (inputDevice_ != "auto" && !inputDevice_.isEmpty()) {
            pw_properties_set(props, PW_KEY_TARGET_OBJECT, inputDevice_.toUtf8().constData());
        }
    }

    handle->stream = pw_stream_new(core_, opts.name.toUtf8().constData(), props);
    if (!handle->stream) {
        qCWarning(lcAudio) << "AudioService: Failed to create PipeWire capture stream:" << opts.name;
        pw_thread_loop_unlock(threadLoop_);
        delete handle;
        return nullptr;
    }

    // Set up process callback — userdata is the handle, with its own listener.
    handle->events = {};
    handle->events.version = PW_VERSION_STREAM_EVENTS;
    handle->events.process = &AudioService::onCaptureProcess;
    pw_stream_add_listener(handle->stream, &handle->listener, &handle->events, handle);

    // Build audio format params
    uint8_t paramBuf[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(paramBuf, sizeof(paramBuf));

    struct spa_audio_info_raw rawInfo{};
    rawInfo.format = (opts.bitDepth == 32) ? SPA_AUDIO_FORMAT_S32_LE : SPA_AUDIO_FORMAT_S16_LE;
    rawInfo.rate = static_cast<uint32_t>(opts.sampleRate);
    rawInfo.channels = static_cast<uint32_t>(opts.channels);

    const struct spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);

    pw_stream_flags connectFlags = static_cast<pw_stream_flags>(
        PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS);
    if (opts.autoconnect)
        connectFlags = static_cast<pw_stream_flags>(connectFlags | PW_STREAM_FLAG_AUTOCONNECT);

    int ret = pw_stream_connect(handle->stream,
        PW_DIRECTION_INPUT,
        PW_ID_ANY,
        connectFlags,
        params, 1);

    if (ret < 0) {
        qCWarning(lcAudio) << "AudioService: Failed to connect capture stream:" << opts.name << "error:" << ret;
        spa_hook_remove(&handle->listener);
        pw_stream_destroy(handle->stream);
        pw_thread_loop_unlock(threadLoop_);
        delete handle;
        return nullptr;
    }

    pw_thread_loop_unlock(threadLoop_);

    {
        QMutexLocker lock(&mutex_);
        captures_.append(handle);
    }

    qCInfo(lcAudio) << "AudioService: Opened capture stream" << opts.name
            << opts.sampleRate << "Hz" << opts.channels << "ch" << opts.bitDepth << "bit"
            << (opts.autoconnect ? "(autoconnect)" : "(manual link)");
    return handle;
}

void AudioService::closeCaptureStream(AudioStreamHandle* handle)
{
    closeCaptureStreamHandle(handle);
}

void AudioService::closeCaptureStreamHandle(AudioStreamHandle* handle)
{
    if (!handle) return;

    {
        QMutexLocker lock(&mutex_);
        captures_.removeOne(handle);
    }

    // Disable the RT callback guard first — stops the RT thread from invoking it.
    handle->captureCallbackActive.store(false, std::memory_order_release);

    if (handle->stream && threadLoop_) {
        pw_thread_loop_lock(threadLoop_);
        // Remove the spa_hook BEFORE destroying the stream — otherwise the
        // hook's list pointers dangle.
        spa_hook_remove(&handle->listener);
        pw_stream_destroy(handle->stream);
        pw_thread_loop_unlock(threadLoop_);
    } else if (handle->stream) {
        pw_stream_destroy(handle->stream);
    }
    handle->captureCallback = nullptr;

    qCInfo(lcAudio) << "AudioService: Closed capture stream" << handle->name;
    delete handle;
}

void AudioService::setCaptureCallback(AudioStreamHandle* handle, CaptureCallback cb)
{
    if (!handle) return;

    // Only handles created WITH a pre-connect callback are immutable — refuse
    // those. A legacy handle (no options-path callback) keeps replace/clear
    // semantics even after its first set, so the presence of a callback alone
    // never locks it.
    if (handle->captureCallbackImmutable) {
        qCWarning(lcAudio) << "AudioService::setCaptureCallback: handle" << handle->name
                           << "has an immutable pre-connect capture callback; ignoring";
        return;
    }

    // Legacy path: quiesce the RT guard BEFORE swapping the std::function (so the
    // RT thread never reads a half-updated callback), swap (or clear via a null
    // cb), then re-publish through the atomic guard (false when cleared).
    handle->captureCallbackActive.store(false, std::memory_order_release);
    handle->captureCallback = std::move(cb);
    handle->captureCallbackActive.store(static_cast<bool>(handle->captureCallback),
                                        std::memory_order_release);
}

// ---- Stream activity / ring primitives ----

void AudioService::setStreamActive(AudioStreamHandle* h, bool active)
{
    if (!h || !h->stream || !threadLoop_) return;
    pw_thread_loop_lock(threadLoop_);
    pw_stream_set_active(h->stream, active);
    pw_thread_loop_unlock(threadLoop_);
}

void AudioService::resetStreamRing(AudioStreamHandle* h)
{
    // Precondition: BOTH reader AND writer are quiesced. The caller deactivates
    // PLAYBACK first so the READER (onPlaybackProcess) is quiesced; in the BT
    // A2DP tap the caller ALSO gates its capture callback off (captureEnabled_)
    // before draining, so the WRITER is quiesced too. drain() is a plain
    // read-index catch-up — it is NOT writer-safe: write() advances the read
    // index on overflow (drop-oldest), so an overflowing writer would race
    // drain() for the read index and could overwrite the flush. With the writer
    // gated off the ring cannot overflow under it and drain() is the sole
    // read-index mutator (see AudioRingBuffer::drain()).
    if (!h || !h->ringBuffer) return;
    h->ringBuffer->drain();
}

// ---- Adaptive buffer growth ----

void AudioService::checkAdaptiveBuffers()
{
    if (!adaptiveBuffers_) return;

    QMutexLocker lock(&mutex_);
    for (auto* handle : streams_) {
        uint32_t xruns = handle->underrunCount.exchange(0, std::memory_order_relaxed);
        if (xruns >= 2 && handle->bufferMs < handle->maxBufferMs) {
            int oldMs = handle->bufferMs;
            handle->bufferMs = qMin(handle->bufferMs + 10, handle->maxBufferMs);
            // AudioRingBuffer uses spa_ringbuffer with a fixed backing store —
            // live resize would require draining and reallocating. The grown
            // bufferMs will take effect when the stream is next created.
            qCDebug(lcAudio) << "Buffer grown to" << handle->bufferMs
                    << "ms for stream" << handle->name
                    << "(" << xruns << "xruns, was" << oldMs << "ms)"
                    << "— takes effect on next session";
        }
    }
}

// ---- Device disconnect handling ----

void AudioService::onDeviceRemoved(uint32_t registryId)
{
    Q_UNUSED(registryId);
    // PipeWire/WirePlumber handles the actual rerouting when a target device
    // disappears. Active streams will automatically fall back to the default
    // sink/source. We just log it for diagnostics.
    qCWarning(lcAudio) << "AudioService: Audio device removed (registry id:" << registryId
               << ") — PipeWire will reroute active streams to default";
}

} // namespace oap
