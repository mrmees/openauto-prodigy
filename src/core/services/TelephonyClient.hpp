#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QDBusObjectPath>
#include <QDBusMessage>

class QDBusServiceWatcher;

namespace oap {

/// ObjectManager InterfacesAdded payload type (a{sa{sv}}). Registered with
/// qDBusRegisterMetaType in TelephonyClient::start() — without this QtDBus
/// CANNOT deliver the signal to a slot (a{sa{sv}} is not QVariantMap; the
/// mismatch fails silently). Live-debugged 2026-07-05.
using InterfaceMap = QMap<QString, QVariantMap>;

/// D-Bus client for PipeWire's telephony service (org.pipewire.Telephony,
/// SESSION bus, owned by WirePlumber). Mechanics only — call-state policy
/// lives in PhoneStateService.
///
/// Role reminder: the Pi is the HFP Hands-Free (HF) unit; the phone is the
/// Audio Gateway. The AudioGateway1 object represents the REMOTE PHONE.
///
/// Call1 objects are EPHEMERAL (setup phase only) and are never returned by
/// GetManagedObjects — they are tracked exclusively via InterfacesAdded/
/// InterfacesRemoved (live-verified, HFP decision record §6.4).
class TelephonyClient : public QObject {
    Q_OBJECT
public:
    explicit TelephonyClient(QObject* parent = nullptr);
    ~TelephonyClient() override;

    void start();
    void stop();

    bool available() const { return serviceUp_ && !agPath_.isEmpty(); }
    QString agAddress() const { return agAddress_; }
    QString transportState() const { return transportState_; }
    QString codec() const { return codec_; }

    void dial(const QString& number);
    void answer();
    void hangupAll();
    void sendTones(const QString& tones);
    void setRejectSco(bool reject);

signals:
    void availableChanged(bool available);
    void transportStateChanged(const QString& state);
    void codecChanged(const QString& codec);
    void callSetupStarted(const QString& state, const QString& line, const QString& name);
    void callSetupChanged(const QString& state);
    void callSetupEnded();
    void commandFailed(const QString& op, const QString& message);

private slots:
    void onInterfacesAdded(const QDBusObjectPath& path, const oap::InterfaceMap& interfaces);
    void onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces);
    void onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                             const QStringList& invalidated, const QDBusMessage& msg);

private:
    void onServiceUp();
    void onServiceDown();
    void scanExistingObjects();
    void adoptAg(const QString& path, const QVariantMap& props);
    void adoptTransport(const QString& path, const QVariantMap& props);
    void clearTransport();
    void adoptCall(const QString& path, const QVariantMap& props);
    void applyRejectSco();
    void asyncCall(const QString& op, QDBusMessage msg);

    QDBusServiceWatcher* watcher_ = nullptr;
    bool started_ = false;
    bool serviceUp_ = false;
    bool rejectSco_ = false;

    QString agPath_;
    QString agAddress_;
    QString transportPath_;
    QString transportState_;
    QString codec_;
    QString callPath_;
};

} // namespace oap

Q_DECLARE_METATYPE(oap::InterfaceMap)
