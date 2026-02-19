#include "BtAudioPlugin.hpp"
#include "core/plugin/IHostContext.hpp"
#include <QQmlContext>

namespace oap {
namespace plugins {

BtAudioPlugin::BtAudioPlugin(QObject* parent)
    : QObject(parent)
{
}

BtAudioPlugin::~BtAudioPlugin()
{
    shutdown();
}

bool BtAudioPlugin::initialize(IHostContext* context)
{
    hostContext_ = context;

    // TODO (Task 26): Start BlueZ D-Bus monitoring for A2DP connections
    // TODO (Task 27): Subscribe to AVRCP metadata

    if (hostContext_)
        hostContext_->log(LogLevel::Info, QStringLiteral("Bluetooth Audio plugin initialized"));

    return true;
}

void BtAudioPlugin::shutdown()
{
    // TODO (Task 26): Stop D-Bus monitoring
}

void BtAudioPlugin::onActivated(QQmlContext* context)
{
    if (!context) return;
    context->setContextProperty("BtAudioPlugin", this);
}

void BtAudioPlugin::onDeactivated()
{
    // Child context destroyed by PluginRuntimeContext
}

QUrl BtAudioPlugin::qmlComponent() const
{
    return QUrl(QStringLiteral("qrc:/OpenAutoProdigy/BtAudioView.qml"));
}

QUrl BtAudioPlugin::iconSource() const
{
    return QUrl(QStringLiteral("qrc:/icons/bluetooth-audio.svg"));
}

void BtAudioPlugin::play()
{
    // TODO (Task 27): Send AVRCP play command via BlueZ D-Bus
}

void BtAudioPlugin::pause()
{
    // TODO (Task 27): Send AVRCP pause command via BlueZ D-Bus
}

void BtAudioPlugin::next()
{
    // TODO (Task 27): Send AVRCP next command via BlueZ D-Bus
}

void BtAudioPlugin::previous()
{
    // TODO (Task 27): Send AVRCP previous command via BlueZ D-Bus
}

} // namespace plugins
} // namespace oap
