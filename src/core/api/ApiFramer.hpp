#pragma once

#include <QByteArray>
#include <QList>
#include <QtGlobal>

namespace oap::api {

class ApiFramer {
public:
    explicit ApiFramer(quint32 maxFrameBytes = 262144);
    QList<QByteArray> feed(const QByteArray& chunk);
    bool violated() const;
    static QByteArray encode(const QByteArray& payload);

private:
    QByteArray buffer_;
    quint32 maxFrameBytes_;
    bool violated_ = false;
};

} // namespace oap::api
