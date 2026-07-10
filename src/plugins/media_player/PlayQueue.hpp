#pragma once

#include <QObject>
#include <QRandomGenerator>
#include <QStringList>
#include <QVector>

namespace oap {
namespace plugins {

/// Play-order logic for the local media player. Pure logic, no I/O.
/// The queue is a snapshot of one container (folder, later album/artist);
/// shuffle re-orders traversal without changing the underlying list.
class PlayQueue : public QObject {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentChanged)
    Q_PROPERTY(int count READ count NOTIFY queueChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY shuffleChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)

public:
    enum RepeatMode { RepeatOff = 0, RepeatAll = 1, RepeatOne = 2 };
    Q_ENUM(RepeatMode)

    explicit PlayQueue(QObject* parent = nullptr);

    void setTracks(const QStringList& paths, int startIndex);
    void clear();

    QString currentTrack() const;
    QStringList tracks() const { return tracks_; }
    int currentIndex() const { return currentIndex_; }
    int count() const { return tracks_.size(); }

    bool shuffle() const { return shuffle_; }
    void setShuffle(bool on);
    int repeatMode() const { return repeatMode_; }
    void setRepeatMode(int mode);

    /// Move to the next track. manual=true is the user's next button
    /// (RepeatOne moves anyway); manual=false is end-of-track auto-advance
    /// (RepeatOne replays). Returns false when the queue end is reached
    /// under RepeatOff (current track unchanged).
    bool advance(bool manual);

    /// Move to the previous track. Wraps only under RepeatAll.
    bool retreat();

    /// Jump to an absolute index in tracks() (e.g. user taps a row).
    void jumpTo(int index);

    /// Remove every track whose path startsWith(pathPrefix) — the USB-yank /
    /// safe-eject purge (Codex P1). Filters tracks_ AND order_ IN PLACE,
    /// preserving survivors' relative order and the current traversal position
    /// (NO rebuildOrder(): no reshuffle, no replay-eligibility churn, shuffle
    /// history intact). A surviving current stays current; a removed current
    /// advances to the next survivor in the OLD traversal order, wrapping to
    /// the first survivor when none follow; an emptied queue drops to
    /// currentIndex -1. Returns the number of tracks removed.
    int removeTracksUnder(const QString& pathPrefix);

    /// Test hook: deterministic shuffle order.
    void setShuffleSeed(quint32 seed);

signals:
    void currentChanged();
    void queueChanged();
    void shuffleChanged();
    void repeatModeChanged();

private:
    void rebuildOrder();

    QStringList tracks_;
    QVector<int> order_;     // traversal order (identity when not shuffled)
    int orderPos_ = -1;      // position within order_
    int currentIndex_ = -1;  // index into tracks_
    bool shuffle_ = false;
    int repeatMode_ = RepeatOff;
    QRandomGenerator rng_{QRandomGenerator::global()->generate()};
};

} // namespace plugins
} // namespace oap
