#pragma once

#include <QObject>
#include <QString>

namespace oap {

class IMediaStatusProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString album READ album NOTIFY mediaStatusChanged)
    Q_PROPERTY(int playbackState READ playbackState NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString source READ source NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString appName READ appName NOTIFY mediaStatusChanged)
public:
    using QObject::QObject;

    virtual QString title() const = 0;
    virtual QString artist() const = 0;
    virtual QString album() const = 0;
    virtual int playbackState() const = 0;
    virtual QString source() const = 0;
    virtual QString appName() const = 0;

    Q_INVOKABLE virtual void playPause() = 0;
    Q_INVOKABLE virtual void next() = 0;
    Q_INVOKABLE virtual void previous() = 0;

signals:
    void mediaStatusChanged();
};

} // namespace oap
