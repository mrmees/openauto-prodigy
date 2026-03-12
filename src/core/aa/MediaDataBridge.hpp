#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

// Forward declarations
namespace oap {
namespace plugins { class BtAudioPlugin; }
namespace aa { class AndroidAutoOrchestrator; }
}

namespace oap {
namespace aa {

/// Unified media metadata bridge that merges AA and BT sources with
/// deterministic priority (AA > BT > None). Exposes a single set of
/// Q_PROPERTYs for the Now Playing widget regardless of active source.
class MediaDataBridge : public QObject {
    Q_OBJECT

public:
    enum Source {
        None = 0,
        Bluetooth = 1,
        AndroidAuto = 2
    };
    Q_ENUM(Source)

    Q_PROPERTY(QString title READ title NOTIFY metadataChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY metadataChanged)
    Q_PROPERTY(QString album READ album NOTIFY metadataChanged)
    Q_PROPERTY(int playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(int source READ source NOTIFY sourceChanged)
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY metadataChanged)
    Q_PROPERTY(QString appName READ appName NOTIFY appNameChanged)

    explicit MediaDataBridge(QObject* parent = nullptr);

    // Connect to backends
    void connectToAAOrchestrator(AndroidAutoOrchestrator* orch);
    void connectToBtAudio(oap::plugins::BtAudioPlugin* bt);

    // Playback controls -- delegate to correct backend based on source
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();

    // Property accessors
    QString title() const;
    QString artist() const;
    QString album() const;
    int playbackState() const;
    int source() const;
    bool hasMedia() const;
    QString appName() const;

signals:
    void metadataChanged();
    void playbackStateChanged();
    void sourceChanged();
    void appNameChanged();

private:
    void setSource(Source newSource);
    void emitCurrentMetadata();

    // AA event handlers
    void onAAConnectionChanged();
    void onAAMetadata(const QString& title, const QString& artist,
                      const QString& album, const QByteArray& albumArt);
    void onAAPlaybackState(int state, const QString& appName);

    // BT event handlers
    void onBTMetadataChanged();
    void onBTPlaybackStateChanged();
    void onBTConnectionStateChanged();

    // Read current BT state from plugin properties
    void readBtState();

    Source source_ = None;

    // AA cached state
    QString aaTitle_;
    QString aaArtist_;
    QString aaAlbum_;
    int aaPlaybackState_ = 0;
    QString aaAppName_;
    QByteArray albumArt_;  // stored but not exposed

    // BT cached state
    QString btTitle_;
    QString btArtist_;
    QString btAlbum_;
    int btPlaybackState_ = 0;

    // Backend pointers
    AndroidAutoOrchestrator* orchestrator_ = nullptr;
    oap::plugins::BtAudioPlugin* btPlugin_ = nullptr;
};

} // namespace aa
} // namespace oap
