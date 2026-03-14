#pragma once

#include <QObject>
#include <QString>

namespace oap {

class IProjectionStatusProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(int projectionState READ projectionState NOTIFY projectionStateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
public:
    enum ProjectionState {
        Disconnected = 0,
        WaitingForDevice,
        Connecting,
        Connected,
        Backgrounded
    };
    Q_ENUM(ProjectionState)

    using QObject::QObject;

    virtual int projectionState() const = 0;
    virtual QString statusMessage() const = 0;

signals:
    void projectionStateChanged();
    void statusMessageChanged();
};

} // namespace oap
