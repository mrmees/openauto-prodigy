#include "NightModeService.hpp"

#include "ThemeService.hpp"
#include "core/Logging.hpp"
#include "core/YamlConfig.hpp"
#include "core/aa/GpioNightMode.hpp"
#include "core/aa/NightModeProvider.hpp"
#include "core/aa/TimedNightMode.hpp"

namespace oap {
namespace {

std::unique_ptr<aa::NightModeProvider> makeProvider(YamlConfig* config)
{
    if (!config) {
        qCWarning(lcCore) << "NightModeService has no configuration; using time defaults";
        return std::make_unique<aa::TimedNightMode>();
    }

    const QString source = config->nightModeSource();
    if (source == QStringLiteral("gpio")) {
        return std::make_unique<aa::GpioNightMode>(
            config->nightModeGpioPin(), config->nightModeGpioActiveHigh());
    }

    if (source != QStringLiteral("time")) {
        // Preserve the existing fallback: all non-GPIO values, including
        // "none", use the configured time policy.
        qCWarning(lcCore) << "Night mode source" << source
                          << "falls back to configured time policy";
    }
    return std::make_unique<aa::TimedNightMode>(
        config->nightModeDayStart(), config->nightModeNightStart());
}

} // namespace

NightModeService::NightModeService(YamlConfig* config, ThemeService* themeService,
                                   QObject* parent)
    : NightModeService(makeProvider(config), themeService, parent)
{
}

NightModeService::NightModeService(
    std::unique_ptr<aa::NightModeProvider> provider,
    ThemeService* themeService, QObject* parent)
    : QObject(parent)
    , provider_(std::move(provider))
    , themeService_(themeService)
{
    connectProvider();
}

NightModeService::~NightModeService()
{
    stop();
}

void NightModeService::connectProvider()
{
    if (!provider_)
        return;

    connect(provider_.get(), &aa::NightModeProvider::nightModeChanged,
            this, [this](bool isNight) {
                if (provider_->hasValidState())
                    publishValidState(isNight);
            });
}

void NightModeService::start()
{
    if (started_ || !provider_)
        return;

    started_ = true;
    provider_->start();

    // Some providers intentionally emit only on transitions. Explicitly
    // adopt the first valid value so an initial day value is authoritative.
    if (provider_->hasValidState())
        publishValidState(provider_->isNight());
}

void NightModeService::stop()
{
    if (!started_ || !provider_)
        return;

    provider_->stop();
    started_ = false;
}

void NightModeService::publishValidState(bool isNight)
{
    if (hasValidState_ && currentState_ == isNight)
        return;

    currentState_ = isNight;
    hasValidState_ = true;
    if (themeService_)
        themeService_->setNightMode(isNight);
    emit nightModeChanged(isNight);
}

} // namespace oap
