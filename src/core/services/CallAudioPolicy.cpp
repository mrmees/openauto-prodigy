#include "CallAudioPolicy.hpp"
#include "IProjectionStatusProvider.hpp"

namespace oap {

CallAudioPolicy::CallAudioPolicy(IProjectionStatusProvider* projection,
                                 bool rejectScoDuringAa, QObject* parent)
    : QObject(parent), projection_(projection), enabled_(rejectScoDuringAa)
{
    if (projection_) {
        connect(projection_, &IProjectionStatusProvider::projectionStateChanged,
                this, &CallAudioPolicy::recompute);
        recompute();
    }
}

void CallAudioPolicy::recompute()
{
    const int s = projection_ ? projection_->projectionState() : 0;
    const bool sessionUp = (s == IProjectionStatusProvider::Connected
                            || s == IProjectionStatusProvider::Backgrounded);
    const bool want = enabled_ && sessionUp;
    if (want == wantReject_) return;
    wantReject_ = want;
    emit rejectScoWanted(want);
}

} // namespace oap
