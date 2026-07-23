#include "core/services/EqualizerService.hpp"
#include "core/YamlConfig.hpp"
#include "core/Logging.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
// Save-debounce interval. Shared by scheduleSave() and the flush-failure
// retry in writeToConfig() so both stay in lockstep.
constexpr int kSaveDebounceMs = 2000;
} // namespace

namespace oap {

EqualizerService::EqualizerService(QObject* parent)
    : QObject(parent)
{
    saveTimer_.setSingleShot(true);
    connect(&saveTimer_, &QTimer::timeout, this, &EqualizerService::writeToConfig);

    // Apply default presets
    applyPreset(StreamId::Media, "Flat");
    applyPreset(StreamId::Navigation, "Voice");
    applyPreset(StreamId::System, "Voice");
    restoring_ = false;
}

EqualizerService::EqualizerService(YamlConfig* config, QObject* parent)
    : QObject(parent)
    , config_(config)
{
    saveTimer_.setSingleShot(true);
    connect(&saveTimer_, &QTimer::timeout, this, &EqualizerService::writeToConfig);

    // Apply default presets first
    applyPreset(StreamId::Media, "Flat");
    applyPreset(StreamId::Navigation, "Voice");
    applyPreset(StreamId::System, "Voice");

    // Override from config if available
    if (config_)
        loadFromConfig();
    restoring_ = false;
}

EqualizerService::~EqualizerService()
{
    // Safety net: consumers are expected to releaseEngine() before teardown
    // (RT ordering contract §4.4), but delete any that outlived their owner so
    // the service never leaks heap engines.
    for (auto& s : streams_) {
        qDeleteAll(s.engines);
        s.engines.clear();
    }
}

QString EqualizerService::activePreset(StreamId stream) const
{
    if (!isValidStream(stream)) return {};
    return streamAt(stream).activePreset;
}

void EqualizerService::applyPreset(StreamId stream, const QString& presetName)
{
    if (!isValidStream(stream)) return;
    const auto* gains = findPresetGains(presetName);
    auto& s = streamAt(stream);

    if (gains) {
        for (auto* e : s.engines) e->setAllGains(*gains);
        s.currentGains = *gains;
        s.activePreset = presetName;
    } else {
        // Fall back to Flat
        const auto* flat = findBundledPreset("Flat");
        for (auto* e : s.engines) e->setAllGains(flat->gains);
        s.currentGains = flat->gains;
        s.activePreset = QStringLiteral("Flat");
    }

    emitPresetSignal(stream);
    emit gainsChanged(stream);
    emit gainsChangedForStream(static_cast<int>(stream));
    scheduleSave();
}

void EqualizerService::setGain(StreamId stream, int band, float dB)
{
    if (!isValidStream(stream)) return;
    if (band < 0 || band >= kNumBands) return;
    // Reject non-finite input at the service boundary (design §4.5 / round-2
    // F5) so no caller path (QML, config, future API) can feed NaN downstream.
    if (!std::isfinite(dB)) return;
    // Clamp to the engine range (+-12 dB) HERE, at the boundary, so the stored
    // value, the getters, and the persisted YAML all agree with what the engine
    // actually applies. Without this the service kept the raw value while the
    // engine clamped, so getters/config reported e.g. 20 dB while audio used 12
    // dB, and a restart snapped the UI to 12.
    dB = std::clamp(dB, -12.0f, 12.0f);

    auto& s = streamAt(stream);
    for (auto* e : s.engines) e->setGain(band, dB);
    s.currentGains[band] = dB;
    s.activePreset.clear();

    emitPresetSignal(stream);
    emit gainsChanged(stream);
    emit gainsChangedForStream(static_cast<int>(stream));
    scheduleSave();
}

float EqualizerService::gain(StreamId stream, int band) const
{
    if (!isValidStream(stream)) return 0.0f;
    if (band < 0 || band >= kNumBands) return 0.0f;
    return streamAt(stream).currentGains[band];
}

std::array<float, kNumBands> EqualizerService::gainsForStream(StreamId stream) const
{
    if (!isValidStream(stream)) return {};
    return streamAt(stream).currentGains;
}

void EqualizerService::setBypassed(StreamId stream, bool bypassed)
{
    if (!isValidStream(stream)) return;
    auto& s = streamAt(stream);
    s.bypassed = bypassed;                     // authoritative (§4.4 / round-1 F7)
    for (auto* e : s.engines) e->setBypassed(bypassed);
    emit bypassedChanged(static_cast<int>(stream));
    scheduleSave();                            // round-1 F4 — arm the save debounce
}

bool EqualizerService::isBypassed(StreamId stream) const
{
    if (!isValidStream(stream)) return false;
    return streamAt(stream).bypassed;
}

QStringList EqualizerService::bundledPresetNames() const
{
    QStringList names;
    names.reserve(kBundledPresetCount);
    for (int i = 0; i < kBundledPresetCount; ++i) {
        names.append(QString::fromUtf8(kBundledPresets[i].name));
    }
    return names;
}

QStringList EqualizerService::userPresetNames() const
{
    QStringList names;
    names.reserve(userPresets_.size());
    for (const auto& p : userPresets_) {
        names.append(p.name);
    }
    return names;
}

QString EqualizerService::saveUserPreset(StreamId source, const QString& name)
{
    if (!isValidStream(source)) return {};
    QString presetName = name.isEmpty() ? generateAutoName() : name;

    // An omitted name retains auto-naming; an explicitly blank-looking or
    // bundled name cannot enter the shared preset namespace.
    if (!isValidUserPresetName(presetName)) {
        return {};
    }

    // Check for existing user preset with same name — overwrite it
    for (auto& p : userPresets_) {
        if (p.name == presetName) {
            p.gains = streamAt(source).currentGains;
            emit presetListChanged();
            scheduleSave();
            return presetName;
        }
    }

    userPresets_.append({presetName, streamAt(source).currentGains});
    emit presetListChanged();
    scheduleSave();
    return presetName;
}

bool EqualizerService::deleteUserPreset(const QString& name)
{
    auto it = std::find_if(userPresets_.begin(), userPresets_.end(),
        [&](const UserPreset& p) { return p.name == name; });

    if (it == userPresets_.end()) return false;

    userPresets_.erase(it);

    // Revert any streams using this preset to Flat
    for (int i = 0; i < 3; ++i) {
        if (streams_[i].activePreset == name) {
            auto sid = static_cast<StreamId>(i);
            applyPreset(sid, "Flat");
        }
    }

    emit presetListChanged();
    scheduleSave();
    return true;
}

bool EqualizerService::renameUserPreset(const QString& oldName, const QString& newName)
{
    if (!isValidUserPresetName(newName)) return false;

    auto it = std::find_if(userPresets_.begin(), userPresets_.end(),
        [&](const UserPreset& p) { return p.name == oldName; });

    if (it == userPresets_.end()) return false;
    if (oldName == newName) return true;

    const auto duplicate = std::find_if(userPresets_.cbegin(), userPresets_.cend(),
        [&](const UserPreset& p) { return p.name == newName; });
    if (duplicate != userPresets_.cend()) return false;

    it->name = newName;

    // Update any streams referencing the old name
    for (int i = 0; i < 3; ++i) {
        if (streams_[i].activePreset == oldName) {
            streams_[i].activePreset = newName;
            emitPresetSignal(static_cast<StreamId>(i));
        }
    }

    emit presetListChanged();
    scheduleSave();
    return true;
}

// --- QML-friendly helpers ---

QVariantList EqualizerService::gainsAsList(int streamIndex) const
{
    QVariantList list;
    if (streamIndex < 0 || streamIndex > 2) return list;
    auto sid = static_cast<StreamId>(streamIndex);
    const auto& gains = streamAt(sid).currentGains;
    list.reserve(kNumBands);
    for (int i = 0; i < kNumBands; ++i)
        list.append(static_cast<double>(gains[i]));
    return list;
}

int EqualizerService::bandCount() const
{
    return kNumBands;
}

QString EqualizerService::bandLabel(int band) const
{
    static const char* labels[] = {
        "31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"
    };
    if (band < 0 || band >= kNumBands) return {};
    return QString::fromLatin1(labels[band]);
}

void EqualizerService::setGainForStream(int streamIndex, int band, float dB)
{
    if (streamIndex < 0 || streamIndex > 2) return;
    setGain(static_cast<StreamId>(streamIndex), band, dB);
}

void EqualizerService::setBypassedForStream(int streamIndex, bool bypassed)
{
    if (streamIndex < 0 || streamIndex > 2) return;
    setBypassed(static_cast<StreamId>(streamIndex), bypassed);
}

bool EqualizerService::isBypassedForStream(int streamIndex) const
{
    if (streamIndex < 0 || streamIndex > 2) return false;
    return isBypassed(static_cast<StreamId>(streamIndex));
}

QString EqualizerService::activePresetForStream(int streamIndex) const
{
    if (streamIndex < 0 || streamIndex > 2) return {};
    return activePreset(static_cast<StreamId>(streamIndex));
}

void EqualizerService::applyPresetForStream(int streamIndex, const QString& presetName)
{
    if (streamIndex < 0 || streamIndex > 2) return;
    applyPreset(static_cast<StreamId>(streamIndex), presetName);
}

EqualizerEngine* EqualizerService::acquireEngine(StreamId stream, float sampleRate, int channels)
{
    if (!isValidStream(stream)) return nullptr;
    // Qt owner thread only (see header). Heap-own a fresh instance seeded with
    // the stream's current gains and bypass, then register it for fan-out.
    auto& s = streamAt(stream);
    auto* engine = new EqualizerEngine(sampleRate, channels);
    engine->setAllGains(s.currentGains);
    engine->setBypassed(s.bypassed);
    s.engines.append(engine);
    return engine;
}

void EqualizerService::releaseEngine(EqualizerEngine* engine)
{
    // Qt owner thread only (see header). Find the owning stream, unregister,
    // and delete. Null / unknown pointers are harmless no-ops.
    if (!engine) return;
    for (auto& s : streams_) {
        if (s.engines.removeOne(engine)) {
            delete engine;
            return;
        }
    }
}

// --- Private helpers ---

bool EqualizerService::isValidStream(StreamId stream)
{
    const int index = static_cast<int>(stream);
    return index >= 0 && index < 3;
}

int EqualizerService::streamIndex(StreamId stream) const
{
    return static_cast<int>(stream);
}

const EqualizerService::StreamState& EqualizerService::streamAt(StreamId stream) const
{
    Q_ASSERT(isValidStream(stream));
    return streams_[streamIndex(stream)];
}

EqualizerService::StreamState& EqualizerService::streamAt(StreamId stream)
{
    Q_ASSERT(isValidStream(stream));
    return streams_[streamIndex(stream)];
}

void EqualizerService::emitPresetSignal(StreamId stream)
{
    switch (stream) {
        case StreamId::Media:      emit mediaPresetChanged(); break;
        case StreamId::Navigation: emit navigationPresetChanged(); break;
        case StreamId::System:     emit systemPresetChanged(); break;
    }
}

bool EqualizerService::isBundledName(const QString& name) const
{
    for (int i = 0; i < kBundledPresetCount; ++i) {
        if (name == QString::fromUtf8(kBundledPresets[i].name)) {
            return true;
        }
    }
    return false;
}

bool EqualizerService::isValidUserPresetName(const QString& name) const
{
    return !name.trimmed().isEmpty() && !isBundledName(name);
}

const std::array<float, kNumBands>* EqualizerService::findPresetGains(const QString& name) const
{
    // Check bundled first
    for (int i = 0; i < kBundledPresetCount; ++i) {
        if (name == QString::fromUtf8(kBundledPresets[i].name)) {
            return &kBundledPresets[i].gains;
        }
    }
    // Check user presets
    for (const auto& p : userPresets_) {
        if (p.name == name) {
            return &p.gains;
        }
    }
    return nullptr;
}

QString EqualizerService::generateAutoName() const
{
    for (int n = 1; ; ++n) {
        QString candidate = QStringLiteral("Custom %1").arg(n);
        if (isBundledName(candidate)) continue;

        bool taken = false;
        for (const auto& p : userPresets_) {
            if (p.name == candidate) {
                taken = true;
                break;
            }
        }
        if (!taken) return candidate;
    }
}

// --- Config persistence ---

void EqualizerService::applyRawGains(StreamId stream, const std::array<float, kNumBands>& gains)
{
    // Internal, side-effect-free restore path: set the stored gains and fan out
    // to any live engines WITHOUT clearing the preset name or scheduling a save
    // (loadFromConfig relies on both). Callers set activePreset explicitly.
    auto& s = streamAt(stream);
    s.currentGains = gains;
    for (auto* e : s.engines) e->setAllGains(gains);
}

void EqualizerService::loadFromConfig()
{
    if (!config_) return;

    // Load user presets first (so applyPreset can find them)
    auto cfgPresets = config_->eqUserPresets();
    for (const auto& cp : cfgPresets) {
        const bool duplicate = std::any_of(userPresets_.cbegin(), userPresets_.cend(),
            [&](const UserPreset& existing) { return existing.name == cp.name; });
        if (!isValidUserPresetName(cp.name) || duplicate)
            continue;
        UserPreset up;
        up.name = cp.name;
        up.gains = cp.gains;
        userPresets_.append(up);
    }

    // Restore per-stream state
    static const char* streamNames[] = {"media", "navigation", "system"};
    static const StreamId streamIds[] = {StreamId::Media, StreamId::Navigation, StreamId::System};
    for (int i = 0; i < 3; ++i) {
        const QString name = QString::fromLatin1(streamNames[i]);
        const QString presetName = config_->eqStreamPreset(name);
        if (!presetName.isEmpty()) {
            // Named preset (bundled or user): applyPreset seeds gains from it.
            applyPreset(streamIds[i], presetName);
        } else {
            // Custom (no preset on disk): restore the raw gains verbatim if
            // valid. The stored preset name reflects the empty on-disk value —
            // NOT setGain's per-band clear — and no save is scheduled (§4.5).
            const QList<float> disk = config_->eqStreamGains(name);
            if (static_cast<int>(disk.size()) == kNumBands) {
                std::array<float, kNumBands> g{};
                for (int b = 0; b < kNumBands; ++b) g[b] = disk[b];
                applyRawGains(streamIds[i], g);
            }
            streamAt(streamIds[i]).activePreset = presetName;   // "" ⇒ Custom
        }

        // Bypass is authoritative (§4.4): always restore from disk and fan out
        // to any live engines. Set directly so load stays save-free.
        auto& s = streamAt(streamIds[i]);
        s.bypassed = config_->eqStreamBypassed(name);
        for (auto* e : s.engines) e->setBypassed(s.bypassed);
    }
}

void EqualizerService::scheduleSave()
{
    if (!config_ || restoring_) return;
    dirty_ = true;
    saveTimer_.start(kSaveDebounceMs);
}

void EqualizerService::setFlushHook(FlushFn fn)
{
    flushFn_ = std::move(fn);
}

void EqualizerService::writeToConfig()
{
    if (!config_ || !dirty_) return;

    // Write per-stream preset assignment, raw gains, and authoritative bypass.
    // Gains + bypass are ALWAYS written so a Custom (no-preset) state and a
    // bypass toggle survive a power cut (design §4.5 / round-2 F7).
    static const char* streamNames[] = {"media", "navigation", "system"};
    for (int i = 0; i < 3; ++i) {
        const QString name = QString::fromLatin1(streamNames[i]);
        config_->setEqStreamPreset(name, streams_[i].activePreset);

        QList<float> gains;
        gains.reserve(kNumBands);
        for (int b = 0; b < kNumBands; ++b)
            gains.append(streams_[i].currentGains[b]);
        config_->setEqStreamGains(name, gains);
        config_->setEqStreamBypassed(name, streams_[i].bypassed);
    }

    // Write user presets
    QList<YamlConfig::EqUserPreset> cfgPresets;
    for (const auto& up : userPresets_) {
        YamlConfig::EqUserPreset cp;
        cp.name = up.name;
        cp.gains = up.gains;
        cfgPresets.append(cp);
    }
    config_->setEqUserPresets(cfgPresets);

    // Surface the flush to disk (round-2 F7). On failure, retain the dirty
    // in-memory state and re-arm the debounce so the next tick retries.
    if (flushFn_ && !flushFn_()) {
        qCWarning(lcEq) << "EQ config flush failed; retrying";
        saveTimer_.start(kSaveDebounceMs);
    } else {
        dirty_ = false;
    }
}

void EqualizerService::saveNow()
{
    saveTimer_.stop();
    writeToConfig();
}

} // namespace oap
