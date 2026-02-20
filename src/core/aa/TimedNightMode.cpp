#include "TimedNightMode.hpp"
#include <boost/log/trivial.hpp>

namespace oap {
namespace aa {

TimedNightMode::TimedNightMode(const QString& dayStart,
                               const QString& nightStart,
                               QObject* parent)
    : NightModeProvider(parent)
    , dayStart_(QTime::fromString(dayStart, "HH:mm"))
    , nightStart_(QTime::fromString(nightStart, "HH:mm"))
{
    if (!dayStart_.isValid()) {
        BOOST_LOG_TRIVIAL(warning) << "[TimedNightMode] Invalid dayStart '" << dayStart.toStdString()
                                   << "', defaulting to 07:00";
        dayStart_ = QTime(7, 0);
    }
    if (!nightStart_.isValid()) {
        BOOST_LOG_TRIVIAL(warning) << "[TimedNightMode] Invalid nightStart '" << nightStart.toStdString()
                                   << "', defaulting to 19:00";
        nightStart_ = QTime(19, 0);
    }

    connect(&timer_, &QTimer::timeout, this, &TimedNightMode::evaluate);
}

bool TimedNightMode::isNight() const
{
    return currentState_;
}

void TimedNightMode::start()
{
    BOOST_LOG_TRIVIAL(info) << "[TimedNightMode] Starting — day=" << dayStart_.toString("HH:mm").toStdString()
                            << " night=" << nightStart_.toString("HH:mm").toStdString();
    evaluate();  // Initial check
    timer_.start(60000);  // Poll every 60 seconds
}

void TimedNightMode::stop()
{
    timer_.stop();
}

void TimedNightMode::evaluate()
{
    const QTime now = QTime::currentTime();
    bool night;

    if (nightStart_ > dayStart_) {
        // Normal case: day 07:00-19:00, night 19:00-07:00
        // It's daytime if now is between dayStart and nightStart
        night = !(now >= dayStart_ && now < nightStart_);
    } else {
        // Inverted case: night 02:00, day 10:00
        // It's nighttime if now >= nightStart AND now < dayStart
        night = (now >= nightStart_ && now < dayStart_);
    }

    if (night != currentState_) {
        currentState_ = night;
        BOOST_LOG_TRIVIAL(info) << "[TimedNightMode] Mode changed to " << (night ? "NIGHT" : "DAY")
                                << " (time=" << now.toString("HH:mm").toStdString() << ")";
        emit nightModeChanged(night);
    }
}

} // namespace aa
} // namespace oap
