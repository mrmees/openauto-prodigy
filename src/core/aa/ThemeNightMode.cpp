#include "ThemeNightMode.hpp"
#include "../../core/services/ThemeService.hpp"
#include <boost/log/trivial.hpp>

namespace oap {
namespace aa {

ThemeNightMode::ThemeNightMode(oap::ThemeService* themeService, QObject* parent)
    : NightModeProvider(parent)
    , themeService_(themeService)
{
    if (themeService_) {
        connect(themeService_, &oap::ThemeService::modeChanged, this, [this]() {
            bool night = themeService_->nightMode();
            BOOST_LOG_TRIVIAL(info) << "[ThemeNightMode] Theme mode changed to "
                                    << (night ? "NIGHT" : "DAY");
            emit nightModeChanged(night);
        });
    }
}

bool ThemeNightMode::isNight() const
{
    return themeService_ ? themeService_->nightMode() : false;
}

void ThemeNightMode::start()
{
    BOOST_LOG_TRIVIAL(info) << "[ThemeNightMode] Starting — following ThemeService (current="
                            << (isNight() ? "NIGHT" : "DAY") << ")";
}

void ThemeNightMode::stop()
{
    // Nothing to stop — signal connection is managed by QObject lifetime
}

} // namespace aa
} // namespace oap
