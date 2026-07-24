#include "MediaScanner.hpp"

#include "FolderModel.hpp"
#include "MediaTagReader.hpp"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QSet>
#include <QThread>

#include <memory>
#include <utility>

// Library heuristics informed by Yarock (GPL-3.0, github.com/sebaro/Yarock)
// and Strawberry (GPL-3.0); no code copied.

Q_LOGGING_CATEGORY(lcMediaScanner, "oap.media.scanner")

namespace oap {
namespace plugins {

namespace {
constexpr quint32 kCacheMagic = 0x4F41504C;  // "OAPL"
constexpr quint16 kCacheVersion = 1;

struct CacheEntry {
    qint64 mtimeMs = 0, size = 0;
    MediaTrackInfo info;
    QString artFile;
};

QString sha1Hex16(const QString& s) {
    return QString::fromLatin1(
        QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
}

QDataStream& operator<<(QDataStream& out, const MediaTrackInfo& t) {
    return out << t.title << t.artist << t.albumArtist << t.album << t.genre
               << qint32(t.year) << qint32(t.trackNo) << qint32(t.discNo)
               << t.durationMs << t.hasEmbeddedArt << t.valid;
}
QDataStream& operator>>(QDataStream& in, MediaTrackInfo& t) {
    qint32 year = 0, trackNo = 0, discNo = 0;
    in >> t.title >> t.artist >> t.albumArtist >> t.album >> t.genre
       >> year >> trackNo >> discNo >> t.durationMs >> t.hasEmbeddedArt >> t.valid;
    t.year = year; t.trackNo = trackNo; t.discNo = discNo;
    return in;
}

QString sidecarArt(const QString& dir,
                   const std::function<bool()>& interrupted) {
    // §8 #3: cover|folder|front.{jpg,png}. Probe the six defined candidates
    // directly and observe stop() between them; never materialize the whole
    // removable directory merely to locate a sidecar.
    static const QStringList names = {
        QStringLiteral("cover.jpg"), QStringLiteral("cover.png"),
        QStringLiteral("folder.jpg"), QStringLiteral("folder.png"),
        QStringLiteral("front.jpg"), QStringLiteral("front.png")};
    for (const QString& name : names) {
        if (interrupted()) return {};
        const QFileInfo candidate(QDir(dir).filePath(name));
        if (candidate.isFile()) return candidate.absoluteFilePath();
    }
    return {};
}
} // namespace

MediaScanner::MediaScanner(QObject* parent) : QObject(parent) {
    qRegisterMetaType<QVector<oap::plugins::MediaTrackRecord>>();
    cacheDir_ = QDir::homePath() + QStringLiteral("/.openauto/cache");
}

MediaScanner::~MediaScanner() {
    // The owner should call stop() at its lifecycle boundary. Destruction is a
    // final quiet safety net: do not emit back into a parent being destroyed.
    stopInternal(false);
}

void MediaScanner::setCacheDir(const QString& dir) { cacheDir_ = dir; }

void MediaScanner::setCheckpointHookForTest(CheckpointHook hook) {
    Q_ASSERT(!thread_);
    checkpointHook_ = std::move(hook);
}

QString MediaScanner::rootKeyForPath(const QString& rootPath) {
    const QString canonical = QFileInfo(rootPath).canonicalFilePath();
    return sha1Hex16(canonical.isEmpty() ? rootPath : canonical);
}

void MediaScanner::scan(const QVector<Root>& roots) {
    if (thread_) {
        // Coalesce (Codex P1): newest set wins; in-flight result discarded.
        pendingRoots_ = roots;
        hasPending_ = true;
        return;
    }
    startScan(roots);
}

void MediaScanner::stop() {
    stopInternal(true);
}

void MediaScanner::stopInternal(bool notifyState) {
    Q_ASSERT(QThread::currentThread() == thread());

    hasPending_ = false;
    pendingRoots_.clear();
    ++generation_;  // any already-queued completion now belongs to stale work

    QThread* const worker = std::exchange(thread_, nullptr);
    if (worker) {
        if (cancelToken_) cancelToken_->store(true, std::memory_order_relaxed);
        worker->requestInterruption();
        worker->wait();
        delete worker;
    }
    cancelToken_.reset();

    if (scanning_) {
        scanning_ = false;
        if (notifyState) emit scanningChanged();
    }
}

void MediaScanner::startScan(QVector<Root> roots) {
    if (!scanning_) {
        scanning_ = true;
        emit scanningChanged();
    }
    // shared_ptr, not raw new: the destructor disconnects the completion
    // handler, which must not leak the box (Codex re-run P1) — the lambdas'
    // captured copies release it whichever path runs.
    auto outcome = std::make_shared<ScanOutcome>();
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    cancelToken_ = cancelled;
    const quint64 generation = ++generation_;
    QThread* const worker = QThread::create([this, roots, outcome, cancelled]() {
        runScan(roots, outcome.get(), cancelled);  // worker fills; NO signal here
    });
    thread_ = worker;
    connect(worker, &QThread::finished, this, [this, outcome, worker, generation]() {
        // stop() may have synchronously joined/deleted this worker while its
        // queued completion was already in flight, or a newer scan may own
        // thread_. Pointer comparison is identity only; never dereference the
        // stale worker on this branch.
        if (generation != generation_ || thread_ != worker) return;
        worker->deleteLater();
        thread_ = nullptr;
        cancelToken_.reset();
        lastScanTagReads_ = outcome->tagReads;
        QVector<MediaTrackRecord> records = std::move(outcome->records);
        if (hasPending_) {
            // Stale result: roots changed mid-scan (hot-plug or yank) —
            // discard and rescan the newest set immediately.
            hasPending_ = false;
            startScan(std::exchange(pendingRoots_, {}));
            return;
        }
        scanning_ = false;
        emit scanningChanged();
        emit finished(records);    // busy()==false, scanning()==false here
    });
    worker->start();
}

void MediaScanner::runScan(const QVector<Root>& roots, ScanOutcome* out,
                           const std::shared_ptr<std::atomic_bool>& cancelled) {
    const auto interrupted = [&cancelled] {
        return cancelled->load(std::memory_order_relaxed)
            || QThread::currentThread()->isInterruptionRequested();
    };
    const auto checkpoint = [this, &interrupted](const char* phase) {
        if (checkpointHook_) checkpointHook_(phase);
        return interrupted();
    };
    if (checkpoint("start")) return;
    QDir().mkpath(cacheDir_ + QStringLiteral("/medialib"));
    QDir().mkpath(cacheDir_ + QStringLiteral("/art"));

    for (const Root& root : roots) {
        if (checkpoint("root")) return;
        const QString rootPath = root.path;
        // Root-escape guard baseline (Codex gate re-run P2): resolve the root
        // once so the walk below can reject any directory whose canonical path
        // leaves it — a symlink on an untrusted stick pointing at `/` would
        // otherwise walk the whole filesystem. Empty = unresolvable root; the
        // walk's own canonical-empty check then skips everything for it.
        const QString canonicalRoot = QFileInfo(rootPath).canonicalFilePath();
        const QString cacheFile =
            cacheDir_ + QStringLiteral("/medialib/") + root.key + QStringLiteral(".bin");

        // Load cache. Any stream error discards the WHOLE cache (Codex P2 —
        // never trust a partial load); absence/corruption -> full scan.
        QHash<QString, CacheEntry> cache;
        {
            if (checkpoint("cache")) return;
            QFile f(cacheFile);
            if (f.open(QIODevice::ReadOnly)) {
                QDataStream in(&f);
                in.setVersion(QDataStream::Qt_6_5);
                quint32 magic = 0; quint16 ver = 0; qint32 count = 0;
                in >> magic >> ver >> count;
                if (magic == kCacheMagic && ver == kCacheVersion) {
                    for (qint32 i = 0; i < count; ++i) {
                        if (interrupted()) return;
                        QString path; CacheEntry e;
                        in >> path >> e.mtimeMs >> e.size >> e.info >> e.artFile;
                        if (in.status() != QDataStream::Ok) { cache.clear(); break; }
                        cache.insert(path, e);
                    }
                }
            }
        }

        // Walk: files by extension; visited-dir set kills symlink loops
        // (§8 #4); hidden entries excluded by QDir's default filters.
        const QStringList exts = FolderModel::audioExtensions();
        QStringList files;
        QSet<QString> visitedDirs;
        QVector<QString> pendingDirs{rootPath};
        while (!pendingDirs.isEmpty()) {
            if (checkpoint("traversal")) return;
            const QString dirPath = pendingDirs.takeLast();
            const QString canonical = QFileInfo(dirPath).canonicalFilePath();
            if (canonical.isEmpty() || visitedDirs.contains(canonical)) continue;
            // Stay within the root (Codex gate re-run P2): skip any directory
            // whose canonical path escaped the root via a symlink. Keep the
            // visited-set loop guard below intact.
            if (canonical != canonicalRoot
                && !canonical.startsWith(canonicalRoot + QLatin1Char('/')))
                continue;
            visitedDirs.insert(canonical);
            QDirIterator it(dirPath,
                            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                            QDirIterator::NoIteratorFlags);
            while (it.hasNext()) {
                if (checkpoint("entry")) return;
                const QFileInfo fi = it.nextFileInfo();
                if (fi.isDir()) pendingDirs.append(fi.absoluteFilePath());
                else if (exts.contains(fi.suffix().toLower()))
                    files.append(fi.absoluteFilePath());
            }
        }

        // Read (cache-aware). `vol` holds only VALID records (library +
        // finished()); `invalid` holds valid=false candidates so they can be
        // cached too — otherwise an unchanged corrupt/mislabeled file cache-
        // misses and is re-opened and re-probed on EVERY scan (Codex gate P2).
        QVector<MediaTrackRecord> vol;
        QVector<MediaTrackRecord> invalid;
        int scanned = 0, cacheHits = 0, unreadable = 0, rootTagReads = 0;
        for (const QString& path : files) {
            if (checkpoint("tags")) return;
            const QFileInfo fi(path);
            const qint64 mtime = fi.lastModified().toMSecsSinceEpoch();
            const qint64 size = fi.size();
            MediaTrackRecord rec;
            rec.path = path;
            rec.volumeKey = root.key;
            const auto it = cache.constFind(path);
            if (it != cache.constEnd() && it->mtimeMs == mtime && it->size == size) {
                // Cache hit honors a cached valid=false: no tag read, no vol
                // append — just counted as an (unchanged) unreadable file.
                rec.info = it->info;
                rec.artFile = it->artFile;
                ++cacheHits;
            } else {
                rec.info = MediaTagReader::read(path, cancelled.get());
                if (interrupted()) return;
                ++rootTagReads;
            }
            if (rec.info.valid) vol.append(rec);
            else { invalid.append(rec); ++unreadable; }
            emit progress(++scanned, files.size());
        }

        // Art pass (§8 #3): group by the SHARED bucket key, search the
        // whole group for embedded art (Codex P1 — never lock onto the
        // first record), fall back to sidecar art in the first track's dir.
        QHash<QString, QVector<int>> groups;
        for (int i = 0; i < vol.size(); ++i) {
            if (interrupted()) return;
            groups[mediaAlbumBucketKey(vol[i].info, vol[i].path)].append(i);
        }
        for (auto g = groups.begin(); g != groups.end(); ++g) {
            if (checkpoint("art")) return;
            QString art;
            for (int i : g.value()) {
                if (interrupted()) return;
                if (!vol[i].info.hasEmbeddedArt) continue;
                const QFileInfo src(vol[i].path);
                // mtime in the name self-invalidates retagged art (Codex P2).
                const QString artPath = cacheDir_ + QStringLiteral("/art/")
                    + sha1Hex16(g.key() + vol[i].path
                                + QString::number(src.lastModified().toMSecsSinceEpoch()))
                    + QStringLiteral(".jpg");
                if (QFileInfo::exists(artPath)) { art = artPath; break; }
                const QByteArray bytes =
                    MediaTagReader::embeddedArt(vol[i].path, cancelled.get());
                if (interrupted()) return;
                if (!bytes.isEmpty()) {
                    QSaveFile save(artPath);
                    if (save.open(QIODevice::WriteOnly)) {
                        save.write(bytes);
                        if (save.commit()) { art = artPath; break; }
                    }
                }
            }
            if (interrupted()) return;
            if (art.isEmpty()) {
                art = sidecarArt(QFileInfo(vol[g.value().first()].path).absolutePath(),
                                 interrupted);
                if (interrupted()) return;
            }
            for (int i : g.value()) {
                if (interrupted()) return;
                vol[i].artFile = art;
            }
        }

        // Rewrite cache for this root — atomically (Codex P2: a power cut
        // must never leave a truncated cache). Writes BOTH valid records (with
        // art) and invalid ones (art empty) so nothing is reprobed next scan.
        {
            if (checkpoint("rewrite")) return;
            QSaveFile f(cacheFile);
            if (f.open(QIODevice::WriteOnly)) {
                QDataStream str(&f);
                str.setVersion(QDataStream::Qt_6_5);
                str << kCacheMagic << kCacheVersion
                    << qint32(vol.size() + invalid.size());
                const auto writeRec = [&str](const MediaTrackRecord& rec) {
                    const QFileInfo fi(rec.path);
                    str << rec.path << fi.lastModified().toMSecsSinceEpoch()
                        << fi.size() << rec.info << rec.artFile;
                };
                for (const MediaTrackRecord& rec : vol) {
                    if (interrupted()) return;
                    writeRec(rec);
                }
                for (const MediaTrackRecord& rec : invalid) {
                    if (interrupted()) return;
                    writeRec(rec);
                }
                if (interrupted()) return;
                f.commit();
            }
        }

        qCInfo(lcMediaScanner) << root.label << ": " << files.size() << "files,"
                               << cacheHits << "cache hits," << rootTagReads
                               << "tag reads," << unreadable << "unreadable";
        if (interrupted()) return;
        out->tagReads += rootTagReads;
        out->records += vol;
    }
}

} // namespace plugins
} // namespace oap
