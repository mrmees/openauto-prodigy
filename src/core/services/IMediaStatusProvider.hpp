#pragma once

#include <QObject>
#include <QString>

namespace oap {

class IMediaStatusProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString title READ title NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString album READ album NOTIFY mediaStatusChanged)
    Q_PROPERTY(int playbackState READ playbackState NOTIFY mediaStatusChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString source READ source NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString appName READ appName NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString artUrl READ artUrl NOTIFY mediaStatusChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY progressChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY progressChanged)
    Q_PROPERTY(bool hasPosition READ hasPosition NOTIFY progressChanged)
public:
    using QObject::QObject;

    virtual bool hasMedia() const = 0;
    virtual QString title() const = 0;
    virtual QString artist() const = 0;
    virtual QString album() const = 0;
    /// Raw source-native state (BT 0/1/2, AA 1/2/3, MediaPlayer 0/1/2).
    /// Prefer isPlaying() for UI; consumers mapping this int MUST branch on
    /// source() (see ApiSerializers::buildMediaStatus).
    virtual int playbackState() const = 0;
    virtual QString source() const = 0;
    virtual QString appName() const = 0;

    // Additive surface (2026-07-08 media-player design §6). Default
    // implementations keep existing implementors/fakes compiling.
    virtual bool isPlaying() const { return false; }
    virtual qint64 position() const { return -1; }   ///< ms; -1 = unknown
    virtual qint64 duration() const { return 0; }    ///< ms; 0 = unknown
    virtual bool hasPosition() const { return false; }
    virtual QString artUrl() const { return {}; }    ///< QML-loadable; "" = none

    Q_INVOKABLE virtual void playPause() = 0;
    Q_INVOKABLE virtual void next() = 0;
    Q_INVOKABLE virtual void previous() = 0;

signals:
    void mediaStatusChanged();
    void progressChanged();
};

} // namespace oap
