#include "ui/DisplayInfo.hpp"
#include <cmath>
#include <algorithm>

namespace oap {

DisplayInfo::DisplayInfo(QObject* parent)
    : QObject(parent)
{
    // Compute initial grid dimensions from default 1024x600
    updateGridDimensions();
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

int DisplayInfo::gridColumns() const
{
    return gridColumns_;
}

int DisplayInfo::gridRows() const
{
    return gridRows_;
}

void DisplayInfo::setWindowSize(int w, int h)
{
    if (w <= 0 || h <= 0)
        return;

    if (w == windowWidth_ && h == windowHeight_)
        return;

    windowWidth_ = w;
    windowHeight_ = h;
    updateGridDimensions();
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

void DisplayInfo::updateGridDimensions()
{
    const int marginPx = 40;   // 2 * 20px reference margin
    const int dockPx = 56;     // reference dock height
    int usableW = windowWidth_ - marginPx;
    int usableH = windowHeight_ - marginPx - dockPx;

    const int targetCellW = 150;
    const int targetCellH = 125;

    int newCols = std::clamp(usableW / targetCellW, 3, 8);
    int newRows = std::clamp(usableH / targetCellH, 2, 6);

    if (newCols != gridColumns_ || newRows != gridRows_) {
        gridColumns_ = newCols;
        gridRows_ = newRows;
        emit gridDimensionsChanged();
    }
}

} // namespace oap
