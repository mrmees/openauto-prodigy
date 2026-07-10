#pragma once

#include <QByteArray>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class QDBusServiceWatcher;
class QDBusMessage;

namespace oap {
namespace plugins {

/// D-Bus ObjectManager `a{sa{sv}}` payload (interface name -> properties).
/// Registered with qDBusRegisterMetaType in the watcher ctor — a QVariantMap
/// slot parameter does NOT match this signature and the connect fails at
/// runtime (bench 2026-07-10).
using UsbInterfaceMap = QMap<QString, QVariantMap>;

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

/// Verdict for a single udisks Filesystem candidate given its (cached) Drive
/// identity and Block hint policy. PURE — no D-Bus, no state — so the whole
/// eligibility + hint decision is unit-testable in isolation
/// (tests/test_media_usb_policy.cpp). The watcher recomputes this on every
/// re-evaluation from the current cache, so a settled-later Drive property
/// (Removable arriving true seconds after hot-plug) simply flips the verdict.
enum class UsbCandidateDecision {
    Defer,            ///< Drive identity not settled yet (unknown, or known-but-not-eligible): stay dormant and wait for a Drive PropertiesChanged.
    Reject,           ///< Block.HintIgnore / Block.HintSystem — never touch this device.
    RegisterMounted,  ///< Eligible AND already mounted — register it regardless of HintAuto.
    AutoMount,        ///< Eligible, unmounted, Block.HintAuto — issue Filesystem.Mount.
    TrackOnly,        ///< Eligible, unmounted, but HintAuto=false — keep tracking, do NOT auto-mount.
};

/// Pure eligibility + Block-hint policy (spec items 4-6).
///
/// - HintIgnore / HintSystem are a hard reject, evaluated even before the Drive
///   is known (they come from stable Block props).
/// - Eligibility = `removable == true || connectionBus == "usb"`. The USB
///   fallback matches this product's USB-media intent and avoids relying solely
///   on udisks' self-described "guess" `Removable`.
/// - A drive that is known but not (yet) eligible returns `Defer`, NOT `Reject`:
///   a later Drive PropertiesChanged may flip `Removable` true. An internal disk
///   simply stays dormant until removal, which is the safe outcome.
/// - Mount policy applies only once eligible: an already-mounted volume is
///   registered regardless of HintAuto; an unmounted one is auto-mounted only
///   when HintAuto is set, otherwise merely tracked.
inline UsbCandidateDecision usbCandidateDecision(bool driveKnown, bool removable,
                                                 const QString& connectionBus, bool hintIgnore,
                                                 bool hintSystem, bool hintAuto, bool mounted) {
    if (hintIgnore || hintSystem) return UsbCandidateDecision::Reject;
    if (!driveKnown) return UsbCandidateDecision::Defer;
    const bool eligible = removable || connectionBus == QLatin1String("usb");
    if (!eligible) return UsbCandidateDecision::Defer;
    if (mounted) return UsbCandidateDecision::RegisterMounted;
    return hintAuto ? UsbCandidateDecision::AutoMount : UsbCandidateDecision::TrackOnly;
}

/// Watches udisks2 (`org.freedesktop.UDisks2`) for removable-media hot-plug,
/// automounts newly inserted volumes, and performs safe eject. Mirrors the
/// BlueZ watcher shape in BtAudioPlugin (system bus, QDBusServiceWatcher,
/// ObjectManager InterfacesAdded/Removed, GetManagedObjects initial scan) with
/// manual QDBusArgument demarshalling per src/AGENTS.md.
///
/// Detection is driven by a PERSISTENT ObjectManager cache, not a per-event
/// one-shot join. `drives_` holds each Drive's Removable/CanPowerOff/
/// ConnectionBus (with known-flags), `fsCandidates_` holds each Block+
/// Filesystem object's merged props. Both are seeded by the initial scan and
/// kept current by InterfacesAdded/Removed AND Drive `PropertiesChanged`. udisks
/// exports a hot-plugged drive before probing settles (Removable=false 3 ms in,
/// true seconds later); a one-shot GetAll sampled the transient false and
/// permanently rejected the volume. The cache re-evaluates candidates whenever
/// their Drive property settles, so late-arriving identity is handled without
/// guessing a probe duration (bench 2026-07-10 root cause, gpt-5.6-sol design).
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
    /// CURRENT cached CanPowerOff is true, then ejectCompleted either way. The
    /// plugin runs its own cleanup BEFORE calling this so QMediaPlayer never
    /// holds the file open into Unmount().
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
    // NOTE the parameter type: ObjectManager's InterfacesAdded carries
    // a{sa{sv}}, which QtDBus REFUSES to deliver into a QVariantMap slot —
    // the connect fails at runtime ("Could not connect ... onInterfacesAdded")
    // and hot-plug goes silently deaf (bench 2026-07-10 root cause). The
    // registered QMap<QString,QVariantMap> metatype matches the signature.
    void onInterfacesAdded(const QDBusObjectPath& path,
                           const UsbInterfaceMap& interfaces);
    void onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces);
    // Drive property settlement (spec item 2). Subscribed to
    // org.freedesktop.DBus.Properties.PropertiesChanged (s a{sv} as) from the
    // udisks service on ANY object path; the trailing QDBusMessage carries the
    // sender path so we can select the Drive cache entry. We filter
    // interface_name == org.freedesktop.UDisks2.Drive in the slot. Matches the
    // proven TelephonyClient/BluetoothManager empty-path pattern; QtDBus
    // demarshals the a{sv} into `changed` natively for a QVariantMap slot.
    void onDrivePropertiesChanged(const QString& interfaceName, const QVariantMap& changed,
                                  const QStringList& invalidated, const QDBusMessage& message);

private:
    /// Cached Drive identity. `*Known` flags distinguish "property reported
    /// false" from "not reported yet" so a partial PropertiesChanged (e.g. only
    /// CanPowerOff) never clobbers a previously-known Removable.
    struct DriveState {
        bool removable = false;
        bool canPowerOff = false;
        QString connectionBus;
        bool removableKnown = false;
        bool canPowerOffKnown = false;
        bool connectionBusKnown = false;
    };
    /// A Block+Filesystem object accumulating across split InterfacesAdded
    /// events. `consumed` latches once registration/mount begins so later
    /// property signals cannot emit a duplicate volumeMounted (spec item 5).
    /// `generation` is the liveness token for async callbacks (spec item 7).
    struct FsCandidate {
        QVariantMap block;
        QVariantMap fs;
        QString drivePath;
        bool hasBlock = false;
        bool hasFs = false;
        bool consumed = false;
        quint64 generation = 0;
    };
    /// A registered, currently-served volume. CanPowerOff is intentionally NOT
    /// cached here — eject reads it LIVE from drives_ so an early-probe false is
    /// never frozen (spec item 6).
    struct FsRecord {
        QString mountPath;
        QString uuid;
        QString label;
        QString drivePath;
    };

    void scanExistingObjects();
    void mergeDriveProps(const QString& drivePath, const QVariantMap& props);
    void mergeCandidate(const QString& objPath, const UsbInterfaceMap& interfaces);
    /// Recompute the verdict for one candidate from the current cache and act
    /// on it (defer / reject / register / auto-mount / track).
    void evaluateCandidate(const QString& objPath);
    void reevaluateCandidatesForDrive(const QString& drivePath);
    /// Async Properties.GetAll on a Drive (used when a relevant property is
    /// invalidated). Re-evaluates dependent candidates on completion.
    void refreshDriveAsync(const QString& drivePath);
    void startMount(const QString& objPath, quint64 generation, const QString& uuid,
                    const QString& label, const QString& deviceName, const QString& drivePath);
    /// Mount() lost a race against a competing automounter (pcmanfm/gvfs run on
    /// the Pi desktop image): query the Filesystem's actual MountPoints
    /// asynchronously and register + emit volumeMounted when it IS mounted — a
    /// race loss must never leave a served volume untracked (bench 2026-07-10).
    void reconcileMountedState(const QString& objPath, quint64 generation, const QString& uuid,
                               const QString& label, const QString& deviceName,
                               const QString& drivePath);
    void registerMounted(const QString& objPath, const QString& mount, const QString& uuid,
                         const QString& label, const QString& deviceName, const QString& drivePath);
    /// Liveness guard for async Mount/reconcile callbacks (spec item 7, latent
    /// defect #4): the candidate must still exist AND carry the generation it
    /// had when the call was issued, or the object was removed (and possibly a
    /// new one took its path) in the meantime — do not resurrect it into objects_.
    bool candidateLive(const QString& objPath, quint64 generation) const;
    void emitRemovedForObject(const QString& objPath);
    void emitRemovedForMount(const QString& mountPath);

    QDBusConnection bus_;
    QDBusServiceWatcher* serviceWatcher_ = nullptr;
    bool disabled_ = false;   ///< system bus wholly unavailable
    bool connected_ = false;  ///< ObjectManager + PropertiesChanged signals hooked up
    // Teardown guard (Codex gate P2): stop() disconnects the signals, but
    // outstanding Mount/Unmount/PowerOff/GetAll QDBusPendingCallWatcher
    // callbacks can still fire afterwards. Set in stop(), cleared in start();
    // every async reply lambda early-returns without emitting when set.
    bool stopped_ = true;

    QHash<QString, DriveState> drives_;      ///< drive object path -> cached identity
    QHash<QString, FsCandidate> fsCandidates_;///< block object path -> merged candidate
    QHash<QString, FsRecord> objects_;       ///< object path -> live filesystem record
    QSet<QString> mountInFlight_;            ///< object paths with a Mount() pending
    quint64 nextGeneration_ = 0;             ///< monotonic candidate liveness token source
};

} // namespace plugins
} // namespace oap
