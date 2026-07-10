#pragma once

#include "MediaLibrary.hpp"
#include <QObject>
#include <QVector>

class QThread;

namespace oap {
namespace plugins {

/// Worker-thread library scanner with per-root incremental cache.
/// Cache: <cacheDir>/medialib/<root.key>.bin (QDataStream, QSaveFile).
/// Art:   <cacheDir>/art/<hash>.jpg (§8 amendment #3 priority; grouped by
///        the shared mediaAlbumBucketKey).
/// Library heuristics informed by Yarock (GPL-3.0, github.com/sebaro/Yarock)
/// and Strawberry (GPL-3.0); no code copied.
class MediaScanner : public QObject {
    Q_OBJECT
public:
    struct Root {
        QString label;   // display name (FolderModel root label)
        QString path;    // absolute root path
        QString key;     // caller-supplied identity: udisks IdUUID-derived
                         // for hot-plug volumes, rootKeyForPath() for dirs
    };

    explicit MediaScanner(QObject* parent = nullptr);
    ~MediaScanner() override;

    void setCacheDir(const QString& dir);
    /// Coalescing: while busy, remembers the NEWEST root set; the in-flight
    /// result is discarded on completion and the pending set scans next.
    void scan(const QVector<Root>& roots);
    bool busy() const { return thread_ != nullptr; }
    bool scanning() const { return scanning_; }
    int lastScanTagReads() const { return lastScanTagReads_; }  // test hook

    static QString rootKeyForPath(const QString& rootPath);

signals:
    void scanningChanged();  // start AND completion edges
    void progress(int filesScanned, int filesTotal);
    /// Emitted from the owner thread with busy()==false, scanning()==false,
    /// only for the newest requested root set.
    void finished(QVector<oap::plugins::MediaTrackRecord> records);

private:
    struct ScanOutcome {
        QVector<MediaTrackRecord> records;
        int tagReads = 0;
    };
    void startScan(QVector<Root> roots);
    void runScan(const QVector<Root>& roots, ScanOutcome* out);

    QString cacheDir_;
    QThread* thread_ = nullptr;
    bool scanning_ = false;
    bool hasPending_ = false;
    QVector<Root> pendingRoots_;
    int lastScanTagReads_ = 0;
};

} // namespace plugins
} // namespace oap
