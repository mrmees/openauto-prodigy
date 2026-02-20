#pragma once

#include <QObject>

namespace oap {
namespace aa {

/// Abstract interface for night mode detection.
/// Implementations determine night mode state from different sources
/// (time-based, theme-based, GPIO-based) and emit nightModeChanged
/// when the state transitions.
class NightModeProvider : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual ~NightModeProvider() = default;

    /// Returns true if currently in night mode.
    virtual bool isNight() const = 0;

    /// Begin monitoring for night mode changes.
    virtual void start() = 0;

    /// Stop monitoring.
    virtual void stop() = 0;

signals:
    void nightModeChanged(bool isNight);
};

} // namespace aa
} // namespace oap
