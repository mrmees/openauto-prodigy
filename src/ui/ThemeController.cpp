#include "ui/ThemeController.hpp"

namespace oap {

ThemeController::ThemeController(std::shared_ptr<Configuration> config, QObject *parent)
    : QObject(parent)
    , config_(std::move(config))
{
}

void ThemeController::setMode(ThemeMode mode)
{
    if (mode_ == mode)
        return;
    mode_ = mode;
    emit modeChanged();
    emit colorsChanged();
}

void ThemeController::toggleMode()
{
    setMode(mode_ == ThemeMode::Day ? ThemeMode::Night : ThemeMode::Day);
}

} // namespace oap
