#pragma once

#include <QObject>
#include <QString>

namespace oap {

class IMediaStatusProvider : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    virtual QString title() const = 0;
    virtual QString artist() const = 0;
    virtual QString album() const = 0;
    virtual int playbackState() const = 0;
    virtual QString source() const = 0;
    virtual QString appName() const = 0;

    virtual void playPause() = 0;
    virtual void next() = 0;
    virtual void previous() = 0;

signals:
    void mediaStatusChanged();
};

} // namespace oap
