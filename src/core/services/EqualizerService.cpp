#include "core/services/EqualizerService.hpp"
#include <algorithm>

namespace oap {

EqualizerService::EqualizerService(QObject* parent)
    : QObject(parent)
{
    // Apply default presets
    applyPreset(StreamId::Media, "Flat");
    applyPreset(StreamId::Navigation, "Voice");
    applyPreset(StreamId::Phone, "Voice");
}

QString EqualizerService::activePreset(StreamId stream) const
{
    return streamAt(stream).activePreset;
}

void EqualizerService::applyPreset(StreamId stream, const QString& presetName)
{
    const auto* gains = findPresetGains(presetName);
    auto& s = streamAt(stream);

    if (gains) {
        s.engine.setAllGains(*gains);
        s.currentGains = *gains;
        s.activePreset = presetName;
    } else {
        // Fall back to Flat
        const auto* flat = findBundledPreset("Flat");
        s.engine.setAllGains(flat->gains);
        s.currentGains = flat->gains;
        s.activePreset = QStringLiteral("Flat");
    }

    emitPresetSignal(stream);
    emit gainsChanged(stream);
}

void EqualizerService::setGain(StreamId stream, int band, float dB)
{
    if (band < 0 || band >= kNumBands) return;

    auto& s = streamAt(stream);
    s.engine.setGain(band, dB);
    s.currentGains[band] = dB;
    s.activePreset.clear();

    emitPresetSignal(stream);
    emit gainsChanged(stream);
}

float EqualizerService::gain(StreamId stream, int band) const
{
    if (band < 0 || band >= kNumBands) return 0.0f;
    return streamAt(stream).currentGains[band];
}

std::array<float, kNumBands> EqualizerService::gainsForStream(StreamId stream) const
{
    return streamAt(stream).currentGains;
}

void EqualizerService::setBypassed(StreamId stream, bool bypassed)
{
    streamAt(stream).engine.setBypassed(bypassed);
}

bool EqualizerService::isBypassed(StreamId stream) const
{
    return streamAt(stream).engine.isBypassed();
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
    QString presetName = name.isEmpty() ? generateAutoName() : name;

    // Reject bundled name collisions
    if (isBundledName(presetName)) {
        return {};
    }

    // Check for existing user preset with same name — overwrite it
    for (auto& p : userPresets_) {
        if (p.name == presetName) {
            p.gains = streamAt(source).currentGains;
            emit presetListChanged();
            return presetName;
        }
    }

    userPresets_.append({presetName, streamAt(source).currentGains});
    emit presetListChanged();
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
    return true;
}

bool EqualizerService::renameUserPreset(const QString& oldName, const QString& newName)
{
    if (isBundledName(newName)) return false;

    auto it = std::find_if(userPresets_.begin(), userPresets_.end(),
        [&](const UserPreset& p) { return p.name == oldName; });

    if (it == userPresets_.end()) return false;

    it->name = newName;

    // Update any streams referencing the old name
    for (int i = 0; i < 3; ++i) {
        if (streams_[i].activePreset == oldName) {
            streams_[i].activePreset = newName;
            emitPresetSignal(static_cast<StreamId>(i));
        }
    }

    emit presetListChanged();
    return true;
}

EqualizerEngine* EqualizerService::engineForStream(StreamId stream)
{
    return &streamAt(stream).engine;
}

// --- Private helpers ---

int EqualizerService::streamIndex(StreamId stream) const
{
    return static_cast<int>(stream);
}

const EqualizerService::StreamState& EqualizerService::streamAt(StreamId stream) const
{
    return streams_[streamIndex(stream)];
}

EqualizerService::StreamState& EqualizerService::streamAt(StreamId stream)
{
    return streams_[streamIndex(stream)];
}

void EqualizerService::emitPresetSignal(StreamId stream)
{
    switch (stream) {
        case StreamId::Media:      emit mediaPresetChanged(); break;
        case StreamId::Navigation: emit navigationPresetChanged(); break;
        case StreamId::Phone:      emit phonePresetChanged(); break;
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

} // namespace oap
