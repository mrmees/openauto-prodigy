#include "UsbMediaWatcher.hpp"

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QVector>

Q_LOGGING_CATEGORY(lcUsbWatcher, "oap.mediaplayer.usb")

namespace {

const QString kService = QStringLiteral("org.freedesktop.UDisks2");
const QString kRootPath = QStringLiteral("/org/freedesktop/UDisks2");
const QString kObjectManager = QStringLiteral("org.freedesktop.DBus.ObjectManager");
const QString kPropsIface = QStringLiteral("org.freedesktop.DBus.Properties");
const QString kBlockIface = QStringLiteral("org.freedesktop.UDisks2.Block");
const QString kFilesystemIface = QStringLiteral("org.freedesktop.UDisks2.Filesystem");
const QString kDriveIface = QStringLiteral("org.freedesktop.UDisks2.Drive");

const QString kRemovable = QStringLiteral("Removable");
const QString kCanPowerOff = QStringLiteral("CanPowerOff");
const QString kConnectionBus = QStringLiteral("ConnectionBus");

/// Unwrap a value that may still be wrapped in QDBusVariant. QtDBus normally
/// unwraps a{sv} values, but gpt-5.6-sol flagged "value remaining wrapped in
/// QDBusVariant" as a failure mode that silently reads as false — so read every
/// Drive/Block property through this (cheap belt-and-suspenders).
QVariant unwrapVariant(const QVariant& v) {
    if (v.canConvert<QDBusVariant>()) return v.value<QDBusVariant>().variant();
    return v;
}

bool propBool(const QVariantMap& m, const QString& key) {
    return unwrapVariant(m.value(key)).toBool();
}
QString propString(const QVariantMap& m, const QString& key) {
    return unwrapVariant(m.value(key)).toString();
}

/// Demarshal a bare `a{sv}` reply argument (e.g. Properties.GetAll) into a map.
/// QtDBus delivers a{sv} either as an UNCONVERTED QDBusArgument or as an
/// already-converted QVariantMap depending on backend/path. The original
/// QDBusArgument-only version returned {} for the converted shape — on the Pi
/// that read Removable as false and hot-plug registration silently skipped
/// every volume (bench 2026-07-10, root cause of dead eject + missed yank).
QVariantMap demarshalPropsReply(const QVariant& v) {
    if (v.userType() == QMetaType::QVariantMap) return v.toMap();
    QVariantMap props;
    if (!v.canConvert<QDBusArgument>()) return props;
    QDBusArgument arg = v.value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();
        QString key;
        QDBusVariant val;
        arg >> key >> val;
        props.insert(key, val.variant());
        arg.endMapEntry();
    }
    arg.endMap();
    return props;
}

/// MountPoints (`aay`) -> list of raw byte arrays for parseFirstMountPoint().
QList<QByteArray> extractMountPoints(const QVariant& value) {
    QList<QByteArray> out;
    const QVariant v = unwrapVariant(value);
    if (v.canConvert<QDBusArgument>()) {
        QDBusArgument arg = v.value<QDBusArgument>();
        arg.beginArray();
        while (!arg.atEnd()) {
            QByteArray ba;
            arg >> ba;
            out.append(ba);
        }
        arg.endArray();
    }
    return out;
}

/// Block.PreferredDevice / Block.Device is a single `ay` — return the basename
/// (e.g. "sda1") as a human-friendly fallback label.
QString deviceBasename(const QVariantMap& blockProps) {
    QVariant v = unwrapVariant(blockProps.value(QStringLiteral("PreferredDevice")));
    if (!v.isValid())
        v = unwrapVariant(blockProps.value(QStringLiteral("Device")));
    QByteArray dev;
    if (v.canConvert<QDBusArgument>()) {
        QDBusArgument arg = v.value<QDBusArgument>();
        arg >> dev;
    } else {
        dev = v.toByteArray();
    }
    while (dev.endsWith('\0')) dev.chop(1);
    return QFileInfo(QString::fromUtf8(dev)).fileName();
}

QString drivePathOf(const QVariantMap& blockProps) {
    // The Drive property ('o') reaches us in different variant shapes per
    // delivery path: QDBusObjectPath from the manual GetManagedObjects
    // demarshal, but potentially a plain string or an argument-wrapped path
    // from the registered-metatype InterfacesAdded delivery (bench
    // 2026-07-10 round 3: hot-plug skipped as "unresolved" because only the
    // first shape was handled). Accept all three.
    const QVariant v = unwrapVariant(blockProps.value(QStringLiteral("Drive")));
    if (v.canConvert<QDBusObjectPath>()) {
        const QString p = v.value<QDBusObjectPath>().path();
        if (!p.isEmpty()) return p;
    }
    if (v.userType() == QMetaType::QString) return v.toString();
    if (v.canConvert<QDBusArgument>()) {
        QDBusObjectPath op;
        v.value<QDBusArgument>() >> op;
        return op.path();
    }
    return QString();
}

/// Internal media (the SD-card boot/root partitions) can report Removable=true
/// through their card-reader drive — only track mounts under the removable
/// prefixes the source list itself uses (bench 2026-07-10: sda1 -> /boot/firmware
/// and sda2 -> / self-registered).
bool mountUnderRemovablePrefix(const QString& mount) {
    return mount.startsWith(QLatin1String("/media/"))
           || mount.startsWith(QLatin1String("/run/media/"))
           || mount.startsWith(QLatin1String("/mnt/"));
}

} // namespace

namespace oap {
namespace plugins {

UsbMediaWatcher::UsbMediaWatcher(QObject* parent)
    : QObject(parent), bus_(QDBusConnection::systemBus()) {
    // Without this registration the InterfacesAdded connect below fails at
    // runtime and hot-plug is silently deaf (bench 2026-07-10 root cause).
    qDBusRegisterMetaType<UsbInterfaceMap>();
}

UsbMediaWatcher::~UsbMediaWatcher() {
    stop();
}

void UsbMediaWatcher::start() {
    if (!bus_.isConnected()) {
        qCWarning(lcUsbWatcher)
            << "system D-Bus unavailable — USB hot-plug/eject disabled";
        disabled_ = true;
        return;
    }
    stopped_ = false;   // re-arm async reply lambdas (Codex gate P2)

    // Re-scan when udisks2 (re)appears; drop stale state when it leaves.
    serviceWatcher_ = new QDBusServiceWatcher(
        kService, bus_,
        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
        this);
    connect(serviceWatcher_, &QDBusServiceWatcher::serviceRegistered, this, [this]() {
        qCInfo(lcUsbWatcher) << "udisks2 appeared on the system bus";
        drives_.clear();
        fsCandidates_.clear();
        objects_.clear();
        mountInFlight_.clear();
        scanExistingObjects();
    });
    connect(serviceWatcher_, &QDBusServiceWatcher::serviceUnregistered, this, [this]() {
        qCInfo(lcUsbWatcher) << "udisks2 left the system bus";
        drives_.clear();
        fsCandidates_.clear();
        objects_.clear();
        mountInFlight_.clear();
    });

    // Subscribe BEFORE the initial snapshot (spec item 2) so no event is missed
    // between the GetManagedObjects read and the signal hookup.
    const bool addedOk =
        bus_.connect(kService, kRootPath, kObjectManager, QStringLiteral("InterfacesAdded"),
                     this, SLOT(onInterfacesAdded(QDBusObjectPath, UsbInterfaceMap)));
    const bool removedOk =
        bus_.connect(kService, kRootPath, kObjectManager, QStringLiteral("InterfacesRemoved"),
                     this, SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList)));
    // Drive property settlement: PropertiesChanged (s a{sv} as) from the udisks
    // service on ANY path — empty path + sender-path-via-QDBusMessage, the
    // proven TelephonyClient/BluetoothManager pattern.
    const bool propsOk =
        bus_.connect(kService, QString(), kPropsIface, QStringLiteral("PropertiesChanged"), this,
                     SLOT(onDrivePropertiesChanged(QString, QVariantMap, QStringList, QDBusMessage)));
    if (!addedOk || !removedOk || !propsOk)
        qCWarning(lcUsbWatcher) << "udisks signal connect FAILED (added:" << addedOk
                                << "removed:" << removedOk << "props:" << propsOk
                                << ") — hot-plug will be deaf";
    else
        qCInfo(lcUsbWatcher) << "ObjectManager + Drive PropertiesChanged signals connected";
    connected_ = true;

    // If udisks2 isn't registered yet, stay dormant — the service watcher above
    // fires scanExistingObjects() when it appears — but warn ONCE so its
    // absence is visible on hosts without udisks2 (e.g. the WSL build box).
    QDBusConnectionInterface* iface = bus_.interface();
    if (!iface || !iface->isServiceRegistered(kService).value()) {
        qCWarning(lcUsbWatcher)
            << "udisks2 not available on the system bus — USB hot-plug/eject disabled";
        return;
    }

    scanExistingObjects();
}

void UsbMediaWatcher::stop() {
    // Disarm any outstanding Mount/Unmount/PowerOff/GetAll callbacks BEFORE
    // tearing down: they early-return without emitting once this is set (Codex
    // gate P2).
    stopped_ = true;
    if (connected_) {
        bus_.disconnect(kService, kRootPath, kObjectManager, QStringLiteral("InterfacesAdded"),
                        this, SLOT(onInterfacesAdded(QDBusObjectPath, UsbInterfaceMap)));
        bus_.disconnect(kService, kRootPath, kObjectManager, QStringLiteral("InterfacesRemoved"),
                        this, SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList)));
        bus_.disconnect(kService, QString(), kPropsIface, QStringLiteral("PropertiesChanged"), this,
                        SLOT(onDrivePropertiesChanged(QString, QVariantMap, QStringList, QDBusMessage)));
        connected_ = false;
    }
    delete serviceWatcher_;
    serviceWatcher_ = nullptr;
    drives_.clear();
    fsCandidates_.clear();
    objects_.clear();
    mountInFlight_.clear();
}

void UsbMediaWatcher::scanExistingObjects() {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        kService, kRootPath, kObjectManager, QStringLiteral("GetManagedObjects"));
    QDBusMessage reply = bus_.call(msg, QDBus::Block, 3000);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qCWarning(lcUsbWatcher) << "GetManagedObjects failed:" << reply.errorMessage();
        return;
    }

    // Reply is a{oa{sa{sv}}} — object path -> interface -> properties. Manual
    // demarshal (QDBusArgument::operator>> cannot pull nested QVariantMap;
    // src/AGENTS.md). Seed drives_ and fsCandidates_ directly through the same
    // merge helpers the signal path uses (spec item 1 — no temporary local maps).
    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();

        QDBusObjectPath objPath;
        arg >> objPath;
        const QString path = objPath.path();

        UsbInterfaceMap ifaces;
        const QDBusArgument ifacesArg = qvariant_cast<QDBusArgument>(arg.asVariant());
        ifacesArg.beginMap();
        while (!ifacesArg.atEnd()) {
            ifacesArg.beginMapEntry();

            QString iface;
            ifacesArg >> iface;

            const QDBusArgument propsArg = qvariant_cast<QDBusArgument>(ifacesArg.asVariant());
            QVariantMap props;
            propsArg.beginMap();
            while (!propsArg.atEnd()) {
                propsArg.beginMapEntry();
                QString key;
                QDBusVariant val;
                propsArg >> key >> val;
                props.insert(key, val.variant());
                propsArg.endMapEntry();
            }
            propsArg.endMap();

            ifacesArg.endMapEntry();
            ifaces.insert(iface, props);
        }
        ifacesArg.endMap();

        arg.endMapEntry();

        if (ifaces.contains(kDriveIface))
            mergeDriveProps(path, ifaces.value(kDriveIface));
        if (ifaces.contains(kBlockIface) || ifaces.contains(kFilesystemIface))
            mergeCandidate(path, ifaces);
    }
    arg.endMap();

    qCInfo(lcUsbWatcher) << "initial scan:" << fsCandidates_.size() << "filesystem candidate(s),"
                         << drives_.size() << "drive(s)";
    // Evaluate every seeded candidate now that all drives are cached (a snapshot
    // of keys — evaluateCandidate may consume/mount but never removes entries).
    const QStringList seeded = fsCandidates_.keys();
    for (const QString& objPath : seeded)
        evaluateCandidate(objPath);
}

void UsbMediaWatcher::mergeDriveProps(const QString& drivePath, const QVariantMap& props) {
    if (drivePath.isEmpty()) return;
    DriveState& d = drives_[drivePath];
    if (props.contains(kRemovable)) {
        d.removable = propBool(props, kRemovable);
        d.removableKnown = true;
    }
    if (props.contains(kCanPowerOff)) {
        d.canPowerOff = propBool(props, kCanPowerOff);
        d.canPowerOffKnown = true;
    }
    if (props.contains(kConnectionBus)) {
        d.connectionBus = propString(props, kConnectionBus);
        d.connectionBusKnown = true;
    }
}

void UsbMediaWatcher::mergeCandidate(const QString& objPath, const UsbInterfaceMap& interfaces) {
    const bool isNew = !fsCandidates_.contains(objPath);
    FsCandidate& c = fsCandidates_[objPath];
    if (isNew) c.generation = ++nextGeneration_;

    if (interfaces.contains(kBlockIface)) {
        const QVariantMap b = interfaces.value(kBlockIface);
        for (auto it = b.constBegin(); it != b.constEnd(); ++it)
            c.block.insert(it.key(), it.value());
        c.hasBlock = true;
        const QString dp = drivePathOf(c.block);
        if (!dp.isEmpty()) c.drivePath = dp;
    }
    if (interfaces.contains(kFilesystemIface)) {
        const QVariantMap f = interfaces.value(kFilesystemIface);
        for (auto it = f.constBegin(); it != f.constEnd(); ++it)
            c.fs.insert(it.key(), it.value());
        c.hasFs = true;
    }
}

void UsbMediaWatcher::evaluateCandidate(const QString& objPath) {
    auto it = fsCandidates_.find(objPath);
    if (it == fsCandidates_.end()) return;
    FsCandidate& c = *it;
    if (c.consumed) return;   // registration/mount already begun — no duplicate (spec item 5)

    // Never require Block and Filesystem in the same signal (split-event defect
    // #1); simply wait until both interfaces have merged onto the candidate.
    if (!c.hasBlock || !c.hasFs) {
        qCInfo(lcUsbWatcher) << "candidate deferred:" << objPath
                             << "awaiting interfaces (block" << c.hasBlock << "fs" << c.hasFs << ")";
        return;
    }

    const QString drivePath = c.drivePath;
    const bool driveKnown = !drivePath.isEmpty() && drives_.contains(drivePath);
    const DriveState d = drives_.value(drivePath);

    const bool hintIgnore = propBool(c.block, QStringLiteral("HintIgnore"));
    const bool hintSystem = propBool(c.block, QStringLiteral("HintSystem"));
    const bool hintAuto = propBool(c.block, QStringLiteral("HintAuto"));

    const QString mount =
        parseFirstMountPoint(extractMountPoints(c.fs.value(QStringLiteral("MountPoints"))));
    const bool mounted = !mount.isEmpty();

    const UsbCandidateDecision decision = usbCandidateDecision(
        driveKnown, d.removable, d.connectionBus, hintIgnore, hintSystem, hintAuto, mounted);

    const QString uuid = propString(c.block, QStringLiteral("IdUUID"));
    const QString label = propString(c.block, QStringLiteral("IdLabel"));
    const QString device = deviceBasename(c.block);

    switch (decision) {
    case UsbCandidateDecision::Defer:
        qCInfo(lcUsbWatcher) << "candidate deferred:" << objPath << "drive" << drivePath
                             << "known" << driveKnown << "removable" << d.removable
                             << "(known" << d.removableKnown << ") bus" << d.connectionBus
                             << "— awaiting Drive settle";
        return;
    case UsbCandidateDecision::Reject:
        qCInfo(lcUsbWatcher) << "candidate hint-rejected:" << objPath << "HintIgnore" << hintIgnore
                             << "HintSystem" << hintSystem;
        c.consumed = true;   // Block hints are stable — stop re-evaluating (no spam, no accidental mount)
        return;
    case UsbCandidateDecision::TrackOnly:
        qCInfo(lcUsbWatcher) << "candidate eligible but HintAuto=false and unmounted — tracking,"
                             << "not auto-mounting:" << objPath;
        return;   // stay un-consumed; a later mount-state change could still register it
    case UsbCandidateDecision::RegisterMounted:
        if (!mountUnderRemovablePrefix(mount)) {
            qCInfo(lcUsbWatcher) << objPath << "skipped: mount outside removable prefixes:" << mount;
            c.consumed = true;   // internal partition (e.g. / or /boot/firmware) — don't reconsider
            return;
        }
        c.consumed = true;   // consume BEFORE emit (spec item 5)
        registerMounted(objPath, mount, uuid, label, device, drivePath);
        return;
    case UsbCandidateDecision::AutoMount:
        if (mountInFlight_.contains(objPath)) return;   // no double-mount on races
        qCInfo(lcUsbWatcher) << "candidate eligible via"
                             << (d.removable ? "Removable" : "ConnectionBus=usb")
                             << "— auto-mounting" << objPath;
        c.consumed = true;   // consume at mount-begin (spec item 5)
        mountInFlight_.insert(objPath);
        startMount(objPath, c.generation, uuid, label, device, drivePath);
        return;
    }
}

void UsbMediaWatcher::reevaluateCandidatesForDrive(const QString& drivePath) {
    if (drivePath.isEmpty()) return;
    const QStringList keys = fsCandidates_.keys();   // snapshot — evaluate may mutate values
    for (const QString& objPath : keys) {
        auto it = fsCandidates_.constFind(objPath);
        if (it == fsCandidates_.constEnd() || it->drivePath != drivePath) continue;
        qCInfo(lcUsbWatcher) << "re-evaluating candidate" << objPath << "for drive" << drivePath;
        evaluateCandidate(objPath);
    }
}

void UsbMediaWatcher::refreshDriveAsync(const QString& drivePath) {
    if (drivePath.isEmpty()) return;
    // Async Properties.GetAll — invalidated properties must be re-read, but a
    // synchronous GetAll here would stall the UI thread (Codex P2). NEVER
    // QDBusInterface (synchronous introspection + 25 s default property timeout).
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, drivePath, kPropsIface,
                                                      QStringLiteral("GetAll"));
    msg << kDriveIface;
    auto* watcher = new QDBusPendingCallWatcher(bus_.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, drivePath](QDBusPendingCallWatcher* cw) {
        cw->deleteLater();
        if (stopped_) return;
        const QDBusMessage reply = cw->reply();
        if (reply.type() != QDBusMessage::ReplyMessage) {
            qCWarning(lcUsbWatcher) << "Drive GetAll refresh failed for" << drivePath << ":"
                                    << reply.errorMessage();
            return;
        }
        const QVariantMap props = demarshalPropsReply(reply.arguments().value(0));
        mergeDriveProps(drivePath, props);
        qCInfo(lcUsbWatcher) << "refreshed drive" << drivePath << "removable"
                             << drives_.value(drivePath).removable << "canPowerOff"
                             << drives_.value(drivePath).canPowerOff << "bus"
                             << drives_.value(drivePath).connectionBus;
        reevaluateCandidatesForDrive(drivePath);
    });
}

void UsbMediaWatcher::registerMounted(const QString& objPath, const QString& mount,
                                      const QString& uuid, const QString& label,
                                      const QString& deviceName, const QString& drivePath) {
    objects_.insert(objPath, {mount, uuid, label, drivePath});
    qCInfo(lcUsbWatcher) << "registered mounted volume" << objPath << "at" << mount;
    emit volumeMounted(mount, label.isEmpty() ? deviceName : label, uuid);
}

void UsbMediaWatcher::startMount(const QString& objPath, quint64 generation, const QString& uuid,
                                 const QString& label, const QString& deviceName,
                                 const QString& drivePath) {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        kService, objPath, kFilesystemIface, QStringLiteral("Mount"));
    msg << QVariant::fromValue(QVariantMap());   // options a{sv}

    auto* watcher = new QDBusPendingCallWatcher(bus_.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, objPath, generation, uuid, label, deviceName, drivePath](
                QDBusPendingCallWatcher* cw) {
        cw->deleteLater();
        if (stopped_) return;   // watcher torn down mid-flight — do not emit
        mountInFlight_.remove(objPath);
        // Liveness guard (spec item 7, defect #4): the object may have been
        // removed (or replaced at the same path) since we issued the Mount.
        if (!candidateLive(objPath, generation)) {
            qCInfo(lcUsbWatcher) << "Mount callback for" << objPath
                                 << "— object no longer live, not registering";
            return;
        }
        const QDBusPendingReply<QString> reply = *cw;
        if (reply.isError()) {
            // A competing automounter (the Pi image runs pcmanfm --desktop +
            // gvfs-udisks2-volume-monitor) can win the mount race — our Mount
            // then fails AlreadyMounted but the volume IS mounted and MUST be
            // registered, or eject/yank handling goes blind (bench 2026-07-10).
            qCWarning(lcUsbWatcher) << "Mount failed for" << objPath << ":"
                                    << reply.error().message()
                                    << "— reconciling actual mount state";
            reconcileMountedState(objPath, generation, uuid, label, deviceName, drivePath);
            return;
        }
        const QString mount = reply.value();
        if (mount.isEmpty()) return;
        if (!mountUnderRemovablePrefix(mount)) {
            qCInfo(lcUsbWatcher) << objPath << "mounted outside removable prefixes:" << mount
                                 << "— not registered";
            return;
        }
        registerMounted(objPath, mount, uuid, label, deviceName, drivePath);
        qCInfo(lcUsbWatcher) << "mounted" << objPath << "at" << mount;
    });
}

void UsbMediaWatcher::reconcileMountedState(const QString& objPath, quint64 generation,
                                            const QString& uuid, const QString& label,
                                            const QString& deviceName, const QString& drivePath) {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        kService, objPath, kPropsIface, QStringLiteral("GetAll"));
    msg << kFilesystemIface;
    auto* watcher = new QDBusPendingCallWatcher(bus_.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, objPath, generation, uuid, label, deviceName, drivePath](
                QDBusPendingCallWatcher* cw) {
        cw->deleteLater();
        if (stopped_) return;   // watcher torn down mid-flight — do not emit
        if (!candidateLive(objPath, generation)) {   // removed since (spec item 7, defect #4)
            qCInfo(lcUsbWatcher) << "reconcile callback for" << objPath
                                 << "— object no longer live, not registering";
            return;
        }
        const QDBusMessage reply = cw->reply();
        if (reply.type() != QDBusMessage::ReplyMessage) {
            qCWarning(lcUsbWatcher) << "reconcile GetAll failed for" << objPath << ":"
                                    << reply.errorMessage();
            return;
        }
        const QVariantMap fsProps = demarshalPropsReply(reply.arguments().value(0));
        const QString mount = parseFirstMountPoint(
            extractMountPoints(fsProps.value(QStringLiteral("MountPoints"))));
        if (mount.isEmpty()) {
            qCWarning(lcUsbWatcher) << objPath
                                    << "genuinely unmounted after Mount failure — not registered";
            return;
        }
        if (!mountUnderRemovablePrefix(mount)) {
            qCInfo(lcUsbWatcher) << objPath << "reconciled mount outside removable prefixes:"
                                 << mount << "— not registered";
            return;
        }
        registerMounted(objPath, mount, uuid, label, deviceName, drivePath);
        qCInfo(lcUsbWatcher) << "reconciled" << objPath << "— already mounted at" << mount;
    });
}

void UsbMediaWatcher::ejectMount(const QString& mountPath) {
    if (disabled_) {
        qCWarning(lcUsbWatcher) << "eject requested but watcher disabled:" << mountPath;
        emit ejectCompleted(mountPath, false);
        return;
    }

    QString objPath, drivePath;
    for (auto it = objects_.constBegin(); it != objects_.constEnd(); ++it) {
        if (it->mountPath == mountPath) {
            objPath = it.key();
            drivePath = it->drivePath;
            break;
        }
    }
    if (objPath.isEmpty()) {
        qCWarning(lcUsbWatcher) << "eject: no udisks Filesystem object for" << mountPath;
        emit ejectCompleted(mountPath, false);
        return;
    }
    // CanPowerOff read LIVE from the Drive cache at eject time, NOT a value
    // captured during early hot-plug probing (spec item 6). An early-probe false
    // would otherwise permanently suppress Drive.PowerOff even after the drive
    // settled CanPowerOff=true.
    const bool canPowerOff = drives_.value(drivePath).canPowerOff;

    QDBusMessage msg = QDBusMessage::createMethodCall(
        kService, objPath, kFilesystemIface, QStringLiteral("Unmount"));
    msg << QVariant::fromValue(QVariantMap());   // options a{sv}

    auto* watcher = new QDBusPendingCallWatcher(bus_.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, mountPath, drivePath, canPowerOff](QDBusPendingCallWatcher* cw) {
        cw->deleteLater();
        if (stopped_) return;   // watcher torn down mid-flight — do not emit
        const QDBusPendingReply<> reply = *cw;
        if (reply.isError()) {
            // Still mounted — leave the source in place, report failure.
            qCWarning(lcUsbWatcher) << "Unmount failed for" << mountPath << ":"
                                    << reply.error().message();
            emit ejectCompleted(mountPath, false);
            return;
        }

        // A successful unmount is the removal — udisks does NOT guarantee a
        // following InterfacesRemoved. Emit through the dedupe map.
        emitRemovedForMount(mountPath);

        if (canPowerOff && !drivePath.isEmpty()) {
            QDBusMessage pmsg = QDBusMessage::createMethodCall(
                kService, drivePath, kDriveIface, QStringLiteral("PowerOff"));
            pmsg << QVariant::fromValue(QVariantMap());
            auto* pw = new QDBusPendingCallWatcher(bus_.asyncCall(pmsg), this);
            connect(pw, &QDBusPendingCallWatcher::finished, this,
                    [this, mountPath](QDBusPendingCallWatcher* pcw) {
                pcw->deleteLater();
                if (stopped_) return;   // watcher torn down mid-flight — do not emit
                const QDBusPendingReply<> preply = *pcw;
                if (preply.isError())   // best effort — the unmount already made it safe
                    qCWarning(lcUsbWatcher) << "PowerOff failed for" << mountPath << ":"
                                            << preply.error().message();
                emit ejectCompleted(mountPath, true);
            });
        } else {
            emit ejectCompleted(mountPath, true);
        }
    });
}

bool UsbMediaWatcher::isKnownMount(const QString& mountPath) const {
    QStringList known;
    for (auto it = objects_.constBegin(); it != objects_.constEnd(); ++it) {
        if (it->mountPath == mountPath) return true;
        known << it->mountPath;
    }
    // A miss here disables eject for a volume the app may be serving — make
    // the mismatch diagnosable from the journal (bench 2026-07-10).
    qCWarning(lcUsbWatcher) << "isKnownMount MISS for" << mountPath
                            << "— tracked mounts:" << known;
    return false;
}

bool UsbMediaWatcher::candidateLive(const QString& objPath, quint64 generation) const {
    auto it = fsCandidates_.constFind(objPath);
    return it != fsCandidates_.constEnd() && it->generation == generation;
}

void UsbMediaWatcher::onInterfacesAdded(const QDBusObjectPath& path,
                                        const UsbInterfaceMap& interfaces) {
    const QString objPath = path.path();
    qCInfo(lcUsbWatcher) << "InterfacesAdded" << objPath << "ifaces" << interfaces.keys();

    // Merge whatever arrived — NEVER require Block+Filesystem in one payload
    // (split-event defect #1). A Drive interface lands on its OWN object path.
    if (interfaces.contains(kDriveIface)) {
        mergeDriveProps(objPath, interfaces.value(kDriveIface));
        reevaluateCandidatesForDrive(objPath);   // candidates waiting on this Drive
    }
    if (interfaces.contains(kBlockIface) || interfaces.contains(kFilesystemIface)) {
        mergeCandidate(objPath, interfaces);
        evaluateCandidate(objPath);
    }
}

void UsbMediaWatcher::onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces) {
    const QString objPath = path.path();
    // Drive removal — drop its cache entry (spec item 7).
    if (interfaces.contains(kDriveIface))
        drives_.remove(objPath);
    // Filesystem removal — clean the candidate (invalidates in-flight async
    // callbacks via candidateLive) and any registered object (spec item 7).
    if (interfaces.contains(kFilesystemIface)) {
        mountInFlight_.remove(objPath);
        fsCandidates_.remove(objPath);
        emitRemovedForObject(objPath);
    }
}

void UsbMediaWatcher::onDrivePropertiesChanged(const QString& interfaceName,
                                               const QVariantMap& changed,
                                               const QStringList& invalidated,
                                               const QDBusMessage& message) {
    if (interfaceName != kDriveIface) return;   // only Drive props drive detection
    const QString drivePath = message.path();
    if (drivePath.isEmpty()) return;

    if (!changed.isEmpty()) mergeDriveProps(drivePath, changed);
    qCInfo(lcUsbWatcher) << "Drive PropertiesChanged" << drivePath << "changed" << changed.keys()
                         << "invalidated" << invalidated;

    // A relevant invalidated property must be re-read (its value was dropped,
    // not delivered) — async GetAll, which re-evaluates on completion.
    const bool needRefresh = invalidated.contains(kRemovable)
                             || invalidated.contains(kCanPowerOff)
                             || invalidated.contains(kConnectionBus);
    if (needRefresh) refreshDriveAsync(drivePath);

    // Act on the merged changes now (the async refresh, if any, re-evaluates
    // again when it lands).
    reevaluateCandidatesForDrive(drivePath);
}

void UsbMediaWatcher::emitRemovedForObject(const QString& objPath) {
    auto it = objects_.find(objPath);
    if (it == objects_.end()) {
        qCInfo(lcUsbWatcher) << "removal for untracked object" << objPath
                             << "(already emitted, or was never registered)";
        return;   // already emitted (e.g. via eject) — dedupe
    }
    const QString mount = it->mountPath;
    objects_.erase(it);
    qCInfo(lcUsbWatcher) << "volume removed" << objPath << "was at" << mount;
    if (!mount.isEmpty()) emit volumeRemoved(mount);
}

void UsbMediaWatcher::emitRemovedForMount(const QString& mountPath) {
    for (auto it = objects_.begin(); it != objects_.end(); ++it) {
        if (it->mountPath == mountPath) {
            objects_.erase(it);
            emit volumeRemoved(mountPath);
            return;
        }
    }
    // Not present — a prior signal already emitted removal. Deduped, no re-emit.
}

} // namespace plugins
} // namespace oap
