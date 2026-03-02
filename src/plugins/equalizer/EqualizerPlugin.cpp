#include "EqualizerPlugin.hpp"
#include "core/plugin/IHostContext.hpp"
#include <QQmlContext>

namespace oap {
namespace plugins {

EqualizerPlugin::EqualizerPlugin(QObject* parent)
    : QObject(parent)
{
}

bool EqualizerPlugin::initialize(IHostContext* context)
{
    hostContext_ = context;

    if (hostContext_)
        hostContext_->log(LogLevel::Info, QStringLiteral("Equalizer plugin initialized"));

    return true;
}

void EqualizerPlugin::shutdown()
{
}

void EqualizerPlugin::onActivated(QQmlContext* /*context*/)
{
    // EqualizerService is already a global QML context property — nothing to expose
}

void EqualizerPlugin::onDeactivated()
{
}

QUrl EqualizerPlugin::qmlComponent() const
{
    return QUrl(QStringLiteral("qrc:/OpenAutoProdigy/EqSettings.qml"));
}

} // namespace plugins
} // namespace oap
