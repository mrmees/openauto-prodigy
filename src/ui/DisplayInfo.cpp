#include "ui/DisplayInfo.hpp"
#include <cmath>

namespace oap {

DisplayInfo::DisplayInfo(QObject* parent)
    : QObject(parent)
{
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

void DisplayInfo::setWindowSize(int w, int h)
{
    if (w <= 0 || h <= 0)
        return;

    if (w == windowWidth_ && h == windowHeight_)
        return;

    windowWidth_ = w;
    windowHeight_ = h;
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

} // namespace oap
