#pragma once

#include "NightModeProvider.hpp"

namespace oap {
class ThemeService;
}

namespace oap {
namespace aa {

/// Night mode provider that follows the ThemeService's day/night mode.
/// When the user (or gesture overlay) toggles the theme, AA gets notified.
class ThemeNightMode : public NightModeProvider {
    Q_OBJECT
public:
    explicit ThemeNightMode(oap::ThemeService* themeService, QObject* parent = nullptr);

    bool isNight() const override;
    void start() override;
    void stop() override;

private:
    oap::ThemeService* themeService_;
    QMetaObject::Connection themeConn_;
};

} // namespace aa
} // namespace oap
