#include "ui/DisplayInfo.hpp"

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

} // namespace oap
