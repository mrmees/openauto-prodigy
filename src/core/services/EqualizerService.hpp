#ifndef OAP_EQUALIZER_SERVICE_HPP
#define OAP_EQUALIZER_SERVICE_HPP

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QVariantList>
#include <QTimer>
#include <array>
#include <functional>
#include "core/audio/BiquadFilter.hpp"
#include "core/audio/EqualizerEngine.hpp"
#include "core/audio/EqualizerPresets.hpp"
#include "core/services/IEqualizerService.hpp"

namespace oap { class YamlConfig; }

namespace oap {

/// Qt service layer wrapping 3 EqualizerEngine instances (media, navigation, phone).
/// Manages bundled + user presets and exposes Q_INVOKABLE/Q_PROPERTY for QML.
class EqualizerService : public QObject, public IEqualizerService {
    Q_OBJECT
    Q_PROPERTY(QString mediaPreset READ mediaPreset NOTIFY mediaPresetChanged)
    Q_PROPERTY(QString navigationPreset READ navigationPreset NOTIFY navigationPresetChanged)
    Q_PROPERTY(QString phonePreset READ phonePreset NOTIFY phonePresetChanged)

public:
    explicit EqualizerService(QObject* parent = nullptr);
    explicit EqualizerService(YamlConfig* config, QObject* parent = nullptr);
    ~EqualizerService() override;

    // --- Q_PROPERTY readers ---
    QString mediaPreset() const { return streams_[0].activePreset; }
    QString navigationPreset() const { return streams_[1].activePreset; }
    QString phonePreset() const { return streams_[2].activePreset; }

    // --- IEqualizerService implementation ---
    Q_INVOKABLE QString activePreset(StreamId stream) const override;
    Q_INVOKABLE void applyPreset(StreamId stream, const QString& presetName) override;
    Q_INVOKABLE void setGain(StreamId stream, int band, float dB) override;
    Q_INVOKABLE float gain(StreamId stream, int band) const override;
    Q_INVOKABLE std::array<float, kNumBands> gainsForStream(StreamId stream) const override;
    Q_INVOKABLE void setBypassed(StreamId stream, bool bypassed) override;
    Q_INVOKABLE bool isBypassed(StreamId stream) const override;
    Q_INVOKABLE QStringList bundledPresetNames() const override;
    Q_INVOKABLE QStringList userPresetNames() const override;
    Q_INVOKABLE QString saveUserPreset(StreamId source, const QString& name = {}) override;
    Q_INVOKABLE bool deleteUserPreset(const QString& name) override;
    Q_INVOKABLE bool renameUserPreset(const QString& oldName, const QString& newName) override;

    // --- QML-friendly helpers (int parameters, no StreamId enum registration needed) ---
    Q_INVOKABLE QVariantList gainsAsList(int streamIndex) const;
    Q_INVOKABLE int bandCount() const;
    Q_INVOKABLE QString bandLabel(int band) const;
    Q_INVOKABLE void setGainForStream(int streamIndex, int band, float dB);
    Q_INVOKABLE void setBypassedForStream(int streamIndex, bool bypassed);
    Q_INVOKABLE bool isBypassedForStream(int streamIndex) const;
    Q_INVOKABLE QString activePresetForStream(int streamIndex) const;
    Q_INVOKABLE void applyPresetForStream(int streamIndex, const QString& presetName);

    /// Acquire a dedicated EqualizerEngine instance for a consumer's audio
    /// stream. The engine is heap-owned by the service and initialized from
    /// the StreamId's current gains AND bypass state. Subsequent
    /// setGain/applyPreset/setBypassed on that StreamId fan out to every live
    /// instance. The caller MUST releaseEngine() it once its stream is
    /// destroyed (RT ordering contract, design §4.4). Every consumer owning a
    /// private instance is what removes the shared-Media-engine state
    /// corruption. Qt owner thread only.
    EqualizerEngine* acquireEngine(StreamId stream, float sampleRate, int channels);

    /// Release an engine previously returned by acquireEngine(): drops it from
    /// the fan-out list and deletes it. The stream that used it MUST already be
    /// destroyed/quiesced (RT ordering contract, design §4.4). Passing a null
    /// or unknown pointer is a harmless no-op. Qt owner thread only.
    void releaseEngine(EqualizerEngine* engine);

    /// Flush pending config changes immediately (call on app shutdown)
    void saveNow();

    /// Persist-to-disk hook. writeToConfig() calls this after mutating the
    /// YamlConfig in memory; it returns true on a successful disk flush. On
    /// false, writeToConfig re-arms the debounce so the next tick retries and
    /// the dirty state is retained (design §4.5 / round-2 F7). Not set in tests
    /// that don't exercise persistence ⇒ writeToConfig only mutates in memory.
    using FlushFn = std::function<bool()>;
    void setFlushHook(FlushFn fn);

signals:
    void mediaPresetChanged();
    void navigationPresetChanged();
    void phonePresetChanged();
    void gainsChanged(StreamId stream);
    void gainsChangedForStream(int stream);
    void bypassedChanged(int stream);
    void presetListChanged();

private:
    struct UserPreset {
        QString name;
        std::array<float, kNumBands> gains;
    };

    struct StreamState {
        QString activePreset;
        std::array<float, kNumBands> currentGains{};
        // Live engine instances fanned out to consumers of this StreamId
        // (non-owning list; the service owns and deletes each on releaseEngine).
        QList<EqualizerEngine*> engines;
        // Authoritative bypass state (design §4.4 / round-1 F7): the fan-out
        // list can be empty, so bypass cannot live only in an engine. New
        // engines inherit this at acquire; isBypassed() reads it.
        bool bypassed = false;
    };

    int streamIndex(StreamId stream) const;
    const StreamState& streamAt(StreamId stream) const;
    StreamState& streamAt(StreamId stream);
    void emitPresetSignal(StreamId stream);
    bool isBundledName(const QString& name) const;
    const std::array<float, kNumBands>* findPresetGains(const QString& name) const;
    QString generateAutoName() const;
    void loadFromConfig();
    void scheduleSave();
    void writeToConfig();
    // Restore raw gains for a stream without setGain's side effects (no
    // per-band preset clearing, no scheduleSave). loadFromConfig-only.
    void applyRawGains(StreamId stream, const std::array<float, kNumBands>& gains);

    // Per-stream state (Media / Navigation / Phone). Engine sample-rate and
    // channel counts now travel with each acquireEngine() call, not the state.
    StreamState streams_[3];

    QList<UserPreset> userPresets_;
    YamlConfig* config_ = nullptr;
    FlushFn flushFn_;
    QTimer saveTimer_;
};

} // namespace oap

#endif // OAP_EQUALIZER_SERVICE_HPP
