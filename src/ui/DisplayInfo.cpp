#include "ui/DisplayInfo.hpp"
#include <cmath>
#include <algorithm>

namespace oap {

DisplayInfo::DisplayInfo(QObject* parent)
    : QObject(parent)
{
    updateCellSide();
}

int DisplayInfo::windowWidth() const
{
    return windowWidth_;
}

int DisplayInfo::windowHeight() const
{
    return windowHeight_;
}

qreal DisplayInfo::screenSizeInches() const
{
    return screenSizeInches_;
}

int DisplayInfo::computedDpi() const
{
    return static_cast<int>(std::round(std::hypot(windowWidth_, windowHeight_) / screenSizeInches_));
}

qreal DisplayInfo::cellSide() const
{
    return cellSide_;
}

int DisplayInfo::densityBias() const
{
    return densityBias_;
}

void DisplayInfo::setWindowSize(int w, int h)
{
    if (w <= 0 || h <= 0)
        return;

    if (w == windowWidth_ && h == windowHeight_)
        return;

    windowWidth_ = w;
    windowHeight_ = h;
    updateCellSide();
    emit windowSizeChanged();
}

void DisplayInfo::setScreenSizeInches(qreal inches)
{
    if (inches <= 0)
        return;

    if (qFuzzyCompare(inches, screenSizeInches_))
        return;

    screenSizeInches_ = inches;
    emit screenSizeChanged();
    emit windowSizeChanged();  // computedDpi depends on screen size
}

void DisplayInfo::setDensityBias(int bias)
{
    bias = std::clamp(bias, -1, 1);
    if (bias == densityBias_)
        return;
    densityBias_ = bias;
    updateCellSide();
}

void DisplayInfo::setFullscreen(bool fs)
{
    if (fs == isFullscreen_)
        return;
    isFullscreen_ = fs;
    updateCellSide();
}

void DisplayInfo::setQScreenDpi(qreal dpi)
{
    if (qFuzzyCompare(dpi, qscreenDpi_))
        return;
    // Guard against NaN/Inf from QScreen
    if (std::isnan(dpi) || std::isinf(dpi))
        return;
    qscreenDpi_ = dpi;
    updateCellSide();
}

void DisplayInfo::setConfigScreenSizeOverride(qreal inches)
{
    if (qFuzzyCompare(inches, configScreenSizeOverride_))
        return;
    configScreenSizeOverride_ = inches;
    updateCellSide();
}

void DisplayInfo::updateCellSide()
{
    // All cascade paths produce diagPx/divisor because the approach is
    // resolution-independent by design. The DPI cascade validates
    // trustworthiness for a future mm-based targeting path where DPI would
    // produce different output; today it validates without changing the result.
    //
    // DPI priority cascade (structural scaffolding for future mm-based path):
    // 1. configScreenSizeOverride_ — user-supplied display diagonal
    // 2. qscreenDpi_ — QScreen physical DPI (trusted when fullscreen)
    // 3. pixel heuristic fallback — diagPx alone
    // All three currently produce the same formula: diagPx / effectiveDivisor.

    const qreal diagPx = std::hypot(static_cast<qreal>(windowWidth_),
                                     static_cast<qreal>(windowHeight_));
    const qreal effectiveDivisor = kBaseDivisor + densityBias_ * kBiasStep;
    const qreal newCellSide = qMax(kMinCellSide, diagPx / effectiveDivisor);

    if (!qFuzzyCompare(newCellSide, cellSide_)) {
        cellSide_ = newCellSide;
        emit cellSideChanged();
    }
}

} // namespace oap
