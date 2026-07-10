#include "UsbMediaWatcher.hpp"

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
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

/// Demarshal one interface's `a{sv}` property dict out of an InterfacesAdded
/// payload (or return it directly if Qt already flattened it to a QVariantMap).
QVariantMap propsFor(const QVariantMap& interfaces, const QString& iface) {
    const QVariant v = interfaces.value(iface);
    if (v.canConvert<QDBusArgument>()) {
        QVariantMap props;
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
    return v.toMap();
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
    return blockProps.value(QStringLiteral("Drive")).value<QDBusObjectPath>().path();
}

} // namespace

namespace oap {
namespace plugins {

UsbMediaWatcher::UsbMediaWatcher(QObject* parent)
    : QObject(parent), bus_(QDBusConnection::systemBus()) {}

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

    bus_.connect(kService, kRootPath, kObjectManager, QStringLiteral("InterfacesAdded"),
                 this, SLOT(onInterfacesAdded(QDBusObjectPath, QVariantMap)));
    bus_.connect(kService, kRootPath, kObjectManager, QStringLiteral("InterfacesRemoved"),
                 this, SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList)));
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
    if (connected_) {
        bus_.disconnect(kService, kRootPath, kObjectManager, QStringLiteral("InterfacesAdded"),
                        this, SLOT(onInterfacesAdded(QDBusObjectPath, QVariantMap)));
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

    for (const FsCand& c : cands)
        processFilesystem(c.objPath, c.block, c.fs, drives.value(drivePathOf(c.block)));
}

void UsbMediaWatcher::processFilesystem(const QString& objPath, const QVariantMap& blockProps,
                                        const QVariantMap& fsProps, const DriveInfo& drive) {
    if (!drive.removable) return;   // internal disks are not our business

    const QString mount =
        parseFirstMountPoint(extractMountPoints(fsProps.value(QStringLiteral("MountPoints"))));
    const QString uuid = blockProps.value(QStringLiteral("IdUUID")).toString();
    const QString label = blockProps.value(QStringLiteral("IdLabel")).toString();
    const QString drivePath = drivePathOf(blockProps);

    if (!mount.isEmpty()) {
        objects_.insert(objPath, {mount, uuid, label, drivePath, drive.canPowerOff});
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
        mountInFlight_.remove(objPath);
        const QDBusPendingReply<QString> reply = *cw;
        if (reply.isError()) {
            qCWarning(lcUsbWatcher) << "Mount failed for" << objPath << ":"
                                    << reply.error().message();
            return;
        }
        const QString mount = reply.value();
        if (mount.isEmpty()) return;
        objects_.insert(objPath, {mount, uuid, label, drivePath, canPowerOff});
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

void UsbMediaWatcher::onInterfacesAdded(const QDBusObjectPath& path, const QVariantMap& interfaces) {
    if (!interfaces.contains(kFilesystemIface)) return;
    const QString objPath = path.path();
    const QVariantMap blockProps = propsFor(interfaces, kBlockIface);
    const QVariantMap fsProps = propsFor(interfaces, kFilesystemIface);
    processFilesystem(objPath, blockProps, fsProps, resolveDrive(drivePathOf(blockProps)));
}

void UsbMediaWatcher::onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces) {
    if (!interfaces.contains(kFilesystemIface)) return;
    const QString objPath = path.path();
    mountInFlight_.remove(objPath);
    emitRemovedForObject(objPath);
}

void UsbMediaWatcher::emitRemovedForObject(const QString& objPath) {
    auto it = objects_.find(objPath);
    if (it == objects_.end()) return;   // already emitted (e.g. via eject) — dedupe
    const QString mount = it->mountPath;
    objects_.erase(it);
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
    QDBusInterface iface(kService, drivePath, kDriveIface, bus_);
    if (iface.isValid()) {
        di.removable = iface.property("Removable").toBool();
        di.canPowerOff = iface.property("CanPowerOff").toBool();
    }
    return di;
}

} // namespace plugins
} // namespace oap
