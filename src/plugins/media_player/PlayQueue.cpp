#include "PlayQueue.hpp"

#include <numeric>

namespace oap {
namespace plugins {

PlayQueue::PlayQueue(QObject* parent) : QObject(parent) {}

void PlayQueue::setTracks(const QStringList& paths, int startIndex) {
    tracks_ = paths;
    currentIndex_ = tracks_.isEmpty() ? -1 : qBound(0, startIndex, tracks_.size() - 1);
    rebuildOrder();
    emit queueChanged();
    emit currentChanged();
}

void PlayQueue::clear() {
    tracks_.clear();
    order_.clear();
    orderPos_ = -1;
    currentIndex_ = -1;
    emit queueChanged();
    emit currentChanged();
}

QString PlayQueue::currentTrack() const {
    return (currentIndex_ >= 0 && currentIndex_ < tracks_.size())
        ? tracks_.at(currentIndex_) : QString();
}

void PlayQueue::setShuffle(bool on) {
    if (shuffle_ == on) return;
    shuffle_ = on;
    rebuildOrder();
    emit shuffleChanged();
}

void PlayQueue::setRepeatMode(int mode) {
    if (repeatMode_ == mode) return;
    repeatMode_ = mode;
    emit repeatModeChanged();
}

void PlayQueue::setShuffleSeed(quint32 seed) {
    rng_ = QRandomGenerator(seed);
    if (shuffle_) rebuildOrder();
}

bool PlayQueue::advance(bool manual) {
    if (tracks_.isEmpty()) return false;
    if (repeatMode_ == RepeatOne && !manual) {
        emit currentChanged();  // replay the same track
        return true;
    }
    if (orderPos_ + 1 < order_.size()) {
        ++orderPos_;
    } else if (repeatMode_ == RepeatAll || (repeatMode_ == RepeatOne && manual)) {
        orderPos_ = 0;
    } else {
        return false;
    }
    currentIndex_ = order_.at(orderPos_);
    emit currentChanged();
    return true;
}

bool PlayQueue::retreat() {
    if (tracks_.isEmpty()) return false;
    if (orderPos_ > 0) {
        --orderPos_;
    } else if (repeatMode_ == RepeatAll) {
        orderPos_ = order_.size() - 1;
    } else {
        return false;
    }
    currentIndex_ = order_.at(orderPos_);
    emit currentChanged();
    return true;
}

void PlayQueue::jumpTo(int index) {
    if (index < 0 || index >= tracks_.size()) return;
    currentIndex_ = index;
    orderPos_ = order_.indexOf(index);
    if (orderPos_ < 0) orderPos_ = 0;  // defensive; order_ always covers all indices
    emit currentChanged();
}

int PlayQueue::removeTracksUnder(const QString& pathPrefix) {
    if (tracks_.isEmpty()) return 0;

    const int oldN = tracks_.size();
    QVector<int> oldToNew(oldN, -1);   // old index -> new index, -1 = removed
    QStringList newTracks;
    newTracks.reserve(oldN);
    int removed = 0;
    for (int i = 0; i < oldN; ++i) {
        if (tracks_.at(i).startsWith(pathPrefix)) {
            ++removed;
        } else {
            oldToNew[i] = newTracks.size();
            newTracks.append(tracks_.at(i));
        }
    }
    if (removed == 0) return 0;

    const int oldCurrent = currentIndex_;
    const bool currentSurvives = (oldCurrent >= 0 && oldToNew[oldCurrent] >= 0);

    // If the current track is going away, find its traversal successor in the
    // OLD order_ (the first survivor AFTER the current traversal position)
    // BEFORE we mutate anything. -1 => none follows, so we'll wrap to the
    // first survivor in traversal order.
    int successorOld = -1;
    if (!currentSurvives && oldCurrent >= 0) {
        for (int p = orderPos_ + 1; p < order_.size(); ++p) {
            const int oldIdx = order_.at(p);
            if (oldToNew[oldIdx] >= 0) { successorOld = oldIdx; break; }
        }
    }

    // Filter order_ in place: drop removed entries, remap survivors to their
    // new indices. Relative sequence (shuffle history) is preserved — nothing
    // is reshuffled and already-played entries keep their positions.
    QVector<int> newOrder;
    newOrder.reserve(newTracks.size());
    for (const int e : order_) {
        const int mapped = oldToNew[e];
        if (mapped >= 0) newOrder.append(mapped);
    }

    tracks_ = std::move(newTracks);
    order_ = std::move(newOrder);

    if (tracks_.isEmpty()) {
        currentIndex_ = -1;
        orderPos_ = -1;
    } else if (currentSurvives) {
        currentIndex_ = oldToNew[oldCurrent];
        orderPos_ = order_.indexOf(currentIndex_);
    } else {
        // Current removed: land on the traversal successor, or wrap to the
        // first survivor in traversal order when nothing followed.
        currentIndex_ = (successorOld >= 0) ? oldToNew[successorOld] : order_.first();
        orderPos_ = order_.indexOf(currentIndex_);
    }

    emit queueChanged();
    emit currentChanged();
    return removed;
}

void PlayQueue::rebuildOrder() {
    order_.resize(tracks_.size());
    std::iota(order_.begin(), order_.end(), 0);
    if (shuffle_ && tracks_.size() > 1) {
        for (int i = order_.size() - 1; i > 0; --i) {
            const int j = int(rng_.bounded(quint32(i + 1)));
            std::swap(order_[i], order_[j]);
        }
        // Current track plays first; traversal continues from the shuffled order.
        const int cur = order_.indexOf(currentIndex_);
        if (cur > 0) std::swap(order_[0], order_[cur]);
        orderPos_ = 0;
    } else {
        orderPos_ = qMax(0, currentIndex_);
    }
    if (order_.isEmpty()) orderPos_ = -1;
}

} // namespace plugins
} // namespace oap
