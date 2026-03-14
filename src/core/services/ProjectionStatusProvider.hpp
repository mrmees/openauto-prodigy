#pragma once

#include "IProjectionStatusProvider.hpp"

namespace oap {

/// Wraps an object with connectionState/statusMessage properties (e.g. AndroidAutoOrchestrator)
/// to expose projection status through the narrow IProjectionStatusProvider interface.
class ProjectionStatusProvider : public IProjectionStatusProvider {
    Q_OBJECT
public:
    explicit ProjectionStatusProvider(QObject* source, QObject* parent = nullptr);

    int projectionState() const override;
    QString statusMessage() const override;

private:
    QObject* source_;
};

} // namespace oap
