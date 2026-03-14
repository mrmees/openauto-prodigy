#include "ProjectionStatusProvider.hpp"
#include <QVariant>

namespace oap {

ProjectionStatusProvider::ProjectionStatusProvider(QObject* source, QObject* parent)
    : IProjectionStatusProvider(parent), source_(source)
{
    connect(source, SIGNAL(connectionStateChanged()), this, SIGNAL(projectionStateChanged()));
    connect(source, SIGNAL(statusMessageChanged()), this, SIGNAL(statusMessageChanged()));
}

int ProjectionStatusProvider::projectionState() const {
    return source_->property("connectionState").toInt();
}

QString ProjectionStatusProvider::statusMessage() const {
    return source_->property("statusMessage").toString();
}

} // namespace oap
