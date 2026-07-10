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
const QString kBlockIface = QStringLiteral("org.freedesktop.UDisks2.Block");
const QString kFilesystemIface = QStringLiteral("org.freedesktop.UDisks2.Filesystem");
const QString kDriveIface = QStringLiteral("org.freedesktop.UDisks2.Drive");

// (propsFor removed 2026-07-10: with the registered UsbInterfaceMap slot
// parameter, QtDBus delivers each interface's props as a QVariantMap
// directly — no per-interface demarshal needed in onInterfacesAdded.)

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
QList<QByteArray> extractMountPoints(const QVariant& v) {
    QList<QByteArray> out;
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
    QVariant v = blockProps.value(QStringLiteral("PreferredDevice"));
    if (!v.isValid())
        v = blockProps.value(QStringLiteral("Device"));
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
    const QVariant v = blockProps.value(QStringLiteral("Drive"));
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
        objects_.clear();
        mountInFlight_.clear();
        scanExistingObjects();
    });
    connect(serviceWatcher_, &QDBusServiceWatcher::serviceUnregistered, this, [this]() {
        qCInfo(lcUsbWatcher) << "udisks2 left the system bus";
        objects_.clear();
        mountInFlight_.clear();
    });

    const bool addedOk =
        bus_.connect(kService, kRootPath, kObjectManager, QStringLiteral("InterfacesAdded"),
                     this, SLOT(onInterfacesAdded(QDBusObjectPath, UsbInterfaceMap)));
    const bool removedOk =
        bus_.connect(kService, kRootPath, kObjectManager, QStringLiteral("InterfacesRemoved"),
                     this, SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList)));
    if (!addedOk || !removedOk)
        qCWarning(lcUsbWatcher) << "ObjectManager signal connect FAILED (added:" << addedOk
                                << "removed:" << removedOk << ") — hot-plug will be deaf";
    else
        qCInfo(lcUsbWatcher) << "ObjectManager hot-plug signals connected";
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
    // Disarm any outstanding Mount/Unmount/PowerOff callbacks BEFORE tearing
    // down: they early-return without emitting once this is set (Codex gate P2).
    stopped_ = true;
    if (connected_) {
        bus_.disconnect(kService, kRootPath, kObjectManager, QStringLiteral("InterfacesAdded"),
                        this, SLOT(onInterfacesAdded(QDBusObjectPath, UsbInterfaceMap)));
        bus_.disconnect(kService, kRootPath, kObjectManager, QStringLiteral("InterfacesRemoved"),
                        this, SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList)));
        connected_ = false;
    }
    delete serviceWatcher_;
    serviceWatcher_ = nullptr;
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
    // src/AGENTS.md). Two passes: collect drives, then resolve each filesystem
    // against its drive.
    QHash<QString, DriveInfo> drives;
    struct FsCand {
        QString objPath;
        QVariantMap block;
        QVariantMap fs;
    };
    QVector<FsCand> cands;

    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();

        QDBusObjectPath objPath;
        arg >> objPath;
        const QString path = objPath.path();

        QVariantMap blockProps, fsProps, driveProps;
        bool hasFs = false, hasDrive = false;

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

            if (iface == kBlockIface) blockProps = props;
            else if (iface == kFilesystemIface) { fsProps = props; hasFs = true; }
            else if (iface == kDriveIface) { driveProps = props; hasDrive = true; }
        }
        ifacesArg.endMap();

        arg.endMapEntry();

        if (hasDrive)
            drives.insert(path, {driveProps.value(QStringLiteral("Removable")).toBool(),
                                 driveProps.value(QStringLiteral("CanPowerOff")).toBool()});
        if (hasFs)
            cands.append({path, blockProps, fsProps});
    }
    arg.endMap();

    qCInfo(lcUsbWatcher) << "initial scan:" << cands.size() << "filesystem candidate(s),"
                         << drives.size() << "drive(s)";
    for (const FsCand& c : cands)
        processFilesystem(c.objPath, c.block, c.fs, drives.value(drivePathOf(c.block)));
}

void UsbMediaWatcher::processFilesystem(const QString& objPath, const QVariantMap& blockProps,
                                        const QVariantMap& fsProps, const DriveInfo& drive) {
    if (!drive.removable) {
        // Also the silent exit taken when resolveDrive gets an empty/failed
        // reply — keep it visible (bench 2026-07-10 root-cause lesson).
        qCInfo(lcUsbWatcher) << objPath << "skipped: drive not removable (or unresolved)";
        return;
    }

    const QString mount =
        parseFirstMountPoint(extractMountPoints(fsProps.value(QStringLiteral("MountPoints"))));
    const QString uuid = blockProps.value(QStringLiteral("IdUUID")).toString();
    const QString label = blockProps.value(QStringLiteral("IdLabel")).toString();
    const QString drivePath = drivePathOf(blockProps);

    if (!mount.isEmpty()) {
        // Internal media (the SD-card boot/root partitions) report
        // Removable=true through their card-reader drive — only track mounts
        // under the removable prefixes the source list itself uses (bench
        // 2026-07-10: sda1 -> /boot/firmware and sda2 -> / self-registered).
        if (!mount.startsWith(QLatin1String("/media/"))
            && !mount.startsWith(QLatin1String("/run/media/"))
            && !mount.startsWith(QLatin1String("/mnt/"))) {
            qCInfo(lcUsbWatcher) << objPath << "skipped: mount outside removable prefixes:" << mount;
            return;
        }
        objects_.insert(objPath, {mount, uuid, label, drivePath, drive.canPowerOff});
        qCInfo(lcUsbWatcher) << "registered mounted volume" << objPath << "at" << mount;
        emit volumeMounted(mount, label.isEmpty() ? deviceBasename(blockProps) : label, uuid);
    } else {
        if (mountInFlight_.contains(objPath)) return;   // no double-mount on races
        mountInFlight_.insert(objPath);
        startMount(objPath, uuid, label, deviceBasename(blockProps), drivePath, drive.canPowerOff);
    }
}

void UsbMediaWatcher::startMount(const QString& objPath, const QString& uuid, const QString& label,
                                 const QString& deviceName, const QString& drivePath,
                                 bool canPowerOff) {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        kService, objPath, kFilesystemIface, QStringLiteral("Mount"));
    msg << QVariant::fromValue(QVariantMap());   // options a{sv}

    auto* watcher = new QDBusPendingCallWatcher(bus_.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, objPath, uuid, label, deviceName, drivePath, canPowerOff](
                QDBusPendingCallWatcher* cw) {
        cw->deleteLater();
        if (stopped_) return;   // watcher torn down mid-flight — do not emit
        mountInFlight_.remove(objPath);
        const QDBusPendingReply<QString> reply = *cw;
        if (reply.isError()) {
            // A competing automounter (the Pi image runs pcmanfm --desktop +
            // gvfs-udisks2-volume-monitor) can win the mount race — our Mount
            // then fails AlreadyMounted but the volume IS mounted and MUST be
            // registered, or eject/yank handling goes blind (bench 2026-07-10).
            qCWarning(lcUsbWatcher) << "Mount failed for" << objPath << ":"
                                    << reply.error().message()
                                    << "— reconciling actual mount state";
            reconcileMountedState(objPath, uuid, label, deviceName, drivePath, canPowerOff);
            return;
        }
        const QString mount = reply.value();
        if (mount.isEmpty()) return;
        objects_.insert(objPath, {mount, uuid, label, drivePath, canPowerOff});
        qCInfo(lcUsbWatcher) << "mounted" << objPath << "at" << mount;
        emit volumeMounted(mount, label.isEmpty() ? deviceName : label, uuid);
    });
}

void UsbMediaWatcher::ejectMount(const QString& mountPath) {
    if (disabled_) {
        qCWarning(lcUsbWatcher) << "eject requested but watcher disabled:" << mountPath;
        emit ejectCompleted(mountPath, false);
        return;
    }

    QString objPath, drivePath;
    bool canPowerOff = false;
    for (auto it = objects_.constBegin(); it != objects_.constEnd(); ++it) {
        if (it->mountPath == mountPath) {
            objPath = it.key();
            drivePath = it->drivePath;
            canPowerOff = it->canPowerOff;
            break;
        }
    }
    if (objPath.isEmpty()) {
        qCWarning(lcUsbWatcher) << "eject: no udisks Filesystem object for" << mountPath;
        emit ejectCompleted(mountPath, false);
        return;
    }

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

void UsbMediaWatcher::reconcileMountedState(const QString& objPath, const QString& uuid,
                                            const QString& label, const QString& deviceName,
                                            const QString& drivePath, bool canPowerOff) {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        kService, objPath, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("GetAll"));
    msg << kFilesystemIface;
    auto* watcher = new QDBusPendingCallWatcher(bus_.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, objPath, uuid, label, deviceName, drivePath, canPowerOff](
                QDBusPendingCallWatcher* cw) {
        cw->deleteLater();
        if (stopped_) return;   // watcher torn down mid-flight — do not emit
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
        objects_.insert(objPath, {mount, uuid, label, drivePath, canPowerOff});
        qCInfo(lcUsbWatcher) << "reconciled" << objPath << "— already mounted at" << mount;
        emit volumeMounted(mount, label.isEmpty() ? deviceName : label, uuid);
    });
}

void UsbMediaWatcher::onInterfacesAdded(const QDBusObjectPath& path,
                                        const UsbInterfaceMap& interfaces) {
    if (!interfaces.contains(kFilesystemIface)) return;
    const QString objPath = path.path();
    const QVariantMap blockProps = interfaces.value(kBlockIface);
    const QVariantMap fsProps = interfaces.value(kFilesystemIface);
    const QString drivePath = drivePathOf(blockProps);
    qCInfo(lcUsbWatcher) << "InterfacesAdded" << objPath << "ifaces" << interfaces.keys()
                         << "blockProps" << blockProps.size()
                         << "drive" << drivePath
                         << "(raw type" << blockProps.value(QStringLiteral("Drive")).typeName() << ")";
    processFilesystem(objPath, blockProps, fsProps, resolveDrive(drivePath));
}

void UsbMediaWatcher::onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces) {
    if (!interfaces.contains(kFilesystemIface)) return;
    const QString objPath = path.path();
    mountInFlight_.remove(objPath);
    emitRemovedForObject(objPath);
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

UsbMediaWatcher::DriveInfo UsbMediaWatcher::resolveDrive(const QString& drivePath) const {
    DriveInfo di;
    if (drivePath.isEmpty() || drivePath == QLatin1String("/")) return di;
    // Direct low-level Properties.GetAll with the SAME 3 s bound as the
    // GetManagedObjects scan. NEVER QDBusInterface in this hot-plug path: its
    // constructor does synchronous introspection and property reads default to
    // a 25 s timeout, either of which can stall the UI thread (Codex gate P2).
    QDBusMessage msg = QDBusMessage::createMethodCall(
        kService, drivePath, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("GetAll"));
    msg << kDriveIface;   // interface_name argument
    const QDBusMessage reply = bus_.call(msg, QDBus::Block, 3000);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qCWarning(lcUsbWatcher) << "Drive GetAll failed for" << drivePath << ":"
                                << reply.errorMessage();
        return di;
    }
    const QVariantMap props = demarshalPropsReply(reply.arguments().value(0));
    if (props.isEmpty()) {
        // Never fail silently again — an empty map here disabled ALL hot-plug
        // handling on the Pi with zero log output (bench 2026-07-10).
        qCWarning(lcUsbWatcher) << "Drive GetAll returned no properties for" << drivePath
                                << "(reply arg type:" << reply.arguments().value(0).typeName() << ")";
        return di;
    }
    di.removable = props.value(QStringLiteral("Removable")).toBool();
    di.canPowerOff = props.value(QStringLiteral("CanPowerOff")).toBool();
    qCInfo(lcUsbWatcher) << "drive" << drivePath << "removable" << di.removable
                         << "canPowerOff" << di.canPowerOff;
    return di;
}

} // namespace plugins
} // namespace oap
