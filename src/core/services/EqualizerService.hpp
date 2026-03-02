#ifndef OAP_EQUALIZER_SERVICE_HPP
#define OAP_EQUALIZER_SERVICE_HPP

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QVariantList>
#include <QTimer>
#include <array>
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

    /// Get raw engine pointer for AudioService stream hookup
    EqualizerEngine* engineForStream(StreamId stream);

    /// Flush pending config changes immediately (call on app shutdown)
    void saveNow();

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
        EqualizerEngine engine;
        QString activePreset;
        std::array<float, kNumBands> currentGains{};

        StreamState(float sampleRate, int channels)
            : engine(sampleRate, channels) {}
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

    StreamState streams_[3] = {
        {48000.0f, 2},  // Media: stereo 48kHz
        {48000.0f, 1},  // Navigation: mono 48kHz
        {16000.0f, 1},  // Phone: mono 16kHz
    };

    QList<UserPreset> userPresets_;
    YamlConfig* config_ = nullptr;
    QTimer saveTimer_;
};

} // namespace oap

#endif // OAP_EQUALIZER_SERVICE_HPP
