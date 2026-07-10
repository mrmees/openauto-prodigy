#pragma once

#include <QByteArray>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class QDBusServiceWatcher;

namespace oap {
namespace plugins {

/// UDisks `MountPoints` is `aay` — a list of NUL-terminated byte arrays.
/// Returns the first mount point (trailing NULs stripped) or an empty string
/// when the list is empty. Defined INLINE so tests link against it before the
/// D-Bus glue below exists (Codex P2 — pure, unit-tested helper).
inline QString parseFirstMountPoint(const QList<QByteArray>& mountPoints) {
    if (mountPoints.isEmpty()) return QString();
    QByteArray first = mountPoints.first();
    while (first.endsWith('\0')) first.chop(1);
    return QString::fromUtf8(first);
}

/// Watches udisks2 (`org.freedesktop.UDisks2`) for removable-media hot-plug,
/// automounts newly inserted volumes, and performs safe eject. Mirrors the
/// BlueZ watcher shape in BtAudioPlugin (system bus, QDBusServiceWatcher,
/// ObjectManager InterfacesAdded/Removed, GetManagedObjects initial scan) with
/// manual QDBusArgument demarshalling per src/AGENTS.md.
///
/// ALL mutating udisks calls (Mount/Unmount/PowerOff) are ASYNC via
/// QDBusPendingCallWatcher — they can block for seconds on real hardware and
/// must never stall the UI thread (Codex P2). The one-shot GetManagedObjects
/// scan is a bounded blocking call, matching the sanctioned BlueZ pattern.
///
/// On a host without udisks2 (e.g. the WSL build box) start() logs a single
/// warning and stays dormant — no crash, no repeated noise.
class UsbMediaWatcher : public QObject {
    Q_OBJECT
public:
    explicit UsbMediaWatcher(QObject* parent = nullptr);
    ~UsbMediaWatcher() override;

    /// Hook the system bus + ObjectManager and scan already-mounted volumes.
    /// Logs one warning and disables itself if udisks2 is unavailable.
    void start();
    /// Tear down bus connections and clear state. Safe to call repeatedly.
    void stop();

    /// Safe-eject: async Unmount({}); on success emit volumeRemoved OURSELVES
    /// through the dedupe map (a successful unmount does NOT guarantee an
    /// InterfacesRemoved), then best-effort PowerOff({}) when the Drive's
    /// CanPowerOff is true, then ejectCompleted either way. The plugin runs its
    /// own cleanup BEFORE calling this so QMediaPlayer never holds the file
    /// open into Unmount().
    void ejectMount(const QString& mountPath);

    /// True if mountPath is a currently-tracked udisks Filesystem mount.
    /// Synchronous local lookup in the object map — NO D-Bus. Lets the plugin
    /// reject eject on fstab/NAS mounts under /mnt BEFORE the disruptive purge
    /// (Codex gate re-run P2). Returns false when the watcher is inactive
    /// (disabled/stopped leave the object map empty).
    bool isKnownMount(const QString& mountPath) const;

signals:
    /// uuid: udisks Block.IdUUID (may be empty — the caller falls back to a
    /// path-derived key). Captured at mount time; never recomputed later.
    void volumeMounted(const QString& mountPath, const QString& label,
                       const QString& uuid);
    void volumeRemoved(const QString& mountPath);   ///< deduped via the object map
    void ejectCompleted(const QString& mountPath, bool ok);

private slots:
    void onInterfacesAdded(const QDBusObjectPath& path, const QVariantMap& interfaces);
    void onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces);

private:
    struct DriveInfo {
        bool removable = false;
        bool canPowerOff = false;
    };
    struct FsRecord {
        QString mountPath;
        QString uuid;
        QString label;
        QString drivePath;
        bool canPowerOff = false;
    };

    void scanExistingObjects();
    void processFilesystem(const QString& objPath, const QVariantMap& blockProps,
                           const QVariantMap& fsProps, const DriveInfo& drive);
    void startMount(const QString& objPath, const QString& uuid, const QString& label,
                    const QString& deviceName, const QString& drivePath, bool canPowerOff);
    void emitRemovedForObject(const QString& objPath);
    void emitRemovedForMount(const QString& mountPath);
    DriveInfo resolveDrive(const QString& drivePath) const;

    QDBusConnection bus_;
    QDBusServiceWatcher* serviceWatcher_ = nullptr;
    bool disabled_ = false;   ///< system bus wholly unavailable
    bool connected_ = false;  ///< ObjectManager signals hooked up
    // Teardown guard (Codex gate P2): stop() disconnects the ObjectManager
    // signals, but outstanding Mount/Unmount/PowerOff QDBusPendingCallWatcher
    // callbacks can still fire afterwards. Set in stop(), cleared in start();
    // every async reply lambda early-returns without emitting when set.
    bool stopped_ = true;

    QHash<QString, FsRecord> objects_;   ///< object path -> live filesystem record
    QSet<QString> mountInFlight_;        ///< object paths with a Mount() pending
};

} // namespace plugins
} // namespace oap
