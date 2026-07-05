#include "PhonePlugin.hpp"
#include "core/plugin/IHostContext.hpp"
#include "core/services/IPhoneStateService.hpp"
#include <QQmlContext>
#include <QTimer>

namespace oap {
namespace plugins {

PhonePlugin::PhonePlugin(QObject* parent)
    : QObject(parent)
{
}

PhonePlugin::~PhonePlugin()
{
    shutdown();
}

bool PhonePlugin::initialize(IHostContext* context)
{
    hostContext_ = context;

    endedFlashTimer_ = new QTimer(this);
    endedFlashTimer_->setSingleShot(true);
    endedFlashTimer_->setInterval(1500);
    connect(endedFlashTimer_, &QTimer::timeout, this, [this]() {
        if (callState_ == Ended)
            setUiState(Idle);
    });

    service_ = qobject_cast<IPhoneStateService*>(
        context ? context->callStateProvider() : nullptr);
    if (service_) {
        connect(service_, &ICallStateProvider::callStateChanged,
                this, &PhonePlugin::onProviderStateChanged);
        connect(service_, &IPhoneStateService::callDurationChanged,
                this, &PhonePlugin::callDurationChanged);
        connect(service_, &IPhoneStateService::connectionChanged,
                this, &PhonePlugin::connectionChanged);
        onProviderStateChanged();
    } else if (hostContext_) {
        hostContext_->log(LogLevel::Warning,
            QStringLiteral("Phone: no IPhoneStateService — plugin inert"));
    }

    if (hostContext_)
        hostContext_->log(LogLevel::Info, QStringLiteral("Phone plugin initialized"));
    return true;
}

void PhonePlugin::shutdown()
{
    if (endedFlashTimer_) {
        endedFlashTimer_->stop();
        endedFlashTimer_ = nullptr;
    }
    service_ = nullptr;
}

void PhonePlugin::onProviderStateChanged()
{
    const int ps = service_->callState();
    CallState mapped = Idle;
    switch (ps) {
    case ICallStateProvider::Ringing:  mapped = Ringing; break;
    case ICallStateProvider::Active:   mapped = Active; break;
    case ICallStateProvider::Dialing:
    case ICallStateProvider::Alerting: mapped = Dialing; break;
    case ICallStateProvider::Held:
    case ICallStateProvider::Waiting:  mapped = HeldActive; break;
    default:                           mapped = Idle; break;
    }

    if (mapped == Ringing && callState_ != Ringing)
        emit incomingCall(service_->callerNumber(), service_->callerName());

    if (mapped == Idle
        && (callState_ == Active || callState_ == Dialing || callState_ == Ringing)) {
        setUiState(Ended);                 // brief "Call ended" flash
        endedFlashTimer_->start();
    } else {
        setUiState(mapped);
    }
    emit callInfoChanged();
}

void PhonePlugin::setUiState(CallState state)
{
    if (state == callState_) return;
    callState_ = state;
    emit callStateChanged();
}

QString PhonePlugin::callerNumber() const { return service_ ? service_->callerNumber() : QString(); }
QString PhonePlugin::callerName() const { return service_ ? service_->callerName() : QString(); }
int PhonePlugin::callDuration() const { return service_ ? service_->callDuration() : 0; }
bool PhonePlugin::phoneConnected() const { return service_ && service_->phoneConnected(); }
QString PhonePlugin::deviceName() const { return service_ ? service_->deviceName() : QString(); }

void PhonePlugin::dial(const QString& number)
{
    if (service_ && service_->dial(number) && hostContext_)
        hostContext_->log(LogLevel::Info, QStringLiteral("Phone: Dialing %1").arg(number));
}

void PhonePlugin::answer()
{
    if (service_) service_->answer();
}

void PhonePlugin::hangup()
{
    if (service_) service_->hangup();
}

void PhonePlugin::appendDigit(const QString& digit)
{
    if (callState_ == Active) {
        sendDTMF(digit);
    } else {
        dialedNumber_ += digit;
        emit dialedNumberChanged();
    }
}

void PhonePlugin::clearDialed()
{
    if (dialedNumber_.isEmpty()) return;
    dialedNumber_.chop(1);
    emit dialedNumberChanged();
}

void PhonePlugin::sendDTMF(const QString& tone)
{
    if (service_) service_->sendDtmf(tone);
}

void PhonePlugin::onActivated(QQmlContext* context)
{
    if (!context) return;
    context->setContextProperty("PhonePlugin", this);
}

void PhonePlugin::onDeactivated()
{
    // Child context destroyed by PluginRuntimeContext
}

QUrl PhonePlugin::qmlComponent() const
{
    return QUrl(QStringLiteral("qrc:/OpenAutoProdigy/PhoneView.qml"));
}

QUrl PhonePlugin::iconSource() const
{
    return {};  // Font-based icons — see MaterialIcon.qml ( phone)
}

} // namespace plugins
} // namespace oap
