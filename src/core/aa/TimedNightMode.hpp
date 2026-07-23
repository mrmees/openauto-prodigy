#pragma once

#include "NightModeProvider.hpp"
#include <QTime>
#include <QTimer>
#include <atomic>
#include <functional>

namespace oap {
namespace aa {

class TimedNightModeTestAccess;

/// Time-based night mode provider.
/// Checks QTime::currentTime() against configured day/night start times.
/// Polls every 60 seconds via QTimer.
class TimedNightMode : public NightModeProvider {
    Q_OBJECT
public:
    using Clock = std::function<QTime()>;

    /// @param dayStart   Time string "HH:mm" when day mode begins (e.g. "07:00")
    /// @param nightStart Time string "HH:mm" when night mode begins (e.g. "19:00")
    explicit TimedNightMode(const QString& dayStart = "07:00",
                            const QString& nightStart = "19:00",
                            QObject* parent = nullptr,
                            Clock clock = {});

    bool isNight() const override;
    bool hasValidState() const override;
    void start() override;
    void stop() override;

private:
    friend class TimedNightModeTestAccess;

    void evaluate();

    QTime dayStart_;
    QTime nightStart_;
    Clock clock_;
    QTimer timer_;
    std::atomic<bool> currentState_{false};
    std::atomic<bool> hasValidState_{false};
};

} // namespace aa
} // namespace oap
