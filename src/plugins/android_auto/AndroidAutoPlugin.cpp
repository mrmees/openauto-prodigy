#include "AndroidAutoPlugin.hpp"
#include "core/aa/AndroidAutoService.hpp"
#include "core/aa/EvdevTouchReader.hpp"
#include "core/plugin/IHostContext.hpp"
#include "core/Configuration.hpp"
#include "core/InputDeviceScanner.hpp"
#include "core/services/IConfigService.hpp"
#include <boost/log/trivial.hpp>
#include <QQmlContext>
#include <QFile>

namespace oap {
namespace plugins {

AndroidAutoPlugin::AndroidAutoPlugin(std::shared_ptr<oap::Configuration> config,
                                     oap::YamlConfig* yamlConfig,
                                     QObject* parent)
    : QObject(parent)
    , config_(std::move(config))
    , yamlConfig_(yamlConfig)
{
}

AndroidAutoPlugin::~AndroidAutoPlugin()
{
    shutdown();
}

bool AndroidAutoPlugin::initialize(IHostContext* context)
{
    hostContext_ = context;

    // Create AA service with audio routing through PipeWire
    auto* audioService = context ? context->audioService() : nullptr;
    aaService_ = new oap::aa::AndroidAutoService(config_, audioService, yamlConfig_, this);

    // Connect navigation: emit signals for PluginModel to handle activation/deactivation.
    QObject::connect(aaService_, &oap::aa::AndroidAutoService::connectionStateChanged,
                     this, [this]() {
        using CS = oap::aa::AndroidAutoService;
        auto state = static_cast<CS::ConnectionState>(aaService_->connectionState());
        if (state == CS::Connected) {
            if (touchReader_) touchReader_->grab();
            emit requestActivation();
        } else if (state == CS::Backgrounded) {
            // Exit to car: hide AA view but keep session alive
            if (touchReader_) touchReader_->ungrab();
            emit requestDeactivation();
        } else if (state == CS::Disconnected
                   || state == CS::WaitingForDevice) {
            if (touchReader_) touchReader_->ungrab();
            emit requestDeactivation();
        }
    });

    // Auto-detect or read touch device from config
    QString touchDevice;
    if (hostContext_ && hostContext_->configService()) {
        touchDevice = hostContext_->configService()->value("touch.device").toString();
    }
    if (touchDevice.isEmpty()) {
        touchDevice = oap::InputDeviceScanner::findTouchDevice();
        if (!touchDevice.isEmpty()) {
            BOOST_LOG_TRIVIAL(info) << "[AAPlugin] Auto-detected touch device: "
                                    << touchDevice.toStdString();
        }
    }

    // Read display resolution from config (default: 1024x600)
    int displayW = 1024, displayH = 600;
    if (hostContext_ && hostContext_->configService()) {
        QVariant w = hostContext_->configService()->value("display.width");
        QVariant h = hostContext_->configService()->value("display.height");
        if (w.isValid()) displayW = w.toInt();
        if (h.isValid()) displayH = h.toInt();
    }

    if (!touchDevice.isEmpty() && QFile::exists(touchDevice)) {
        touchReader_ = new oap::aa::EvdevTouchReader(
            aaService_->touchHandler(),
            touchDevice.toStdString(),
            4095, 4095,   // evdev axis range (overridden by EVIOCGABS in reader)
            1280, 720,    // AA touch coordinate space (matches video resolution)
            displayW, displayH,
            this);
        touchReader_->start();
        BOOST_LOG_TRIVIAL(info) << "[AAPlugin] Touch: " << touchDevice.toStdString()
                                << " display=" << displayW << "x" << displayH;

        // Configure sidebar touch exclusion
        if (hostContext_ && hostContext_->configService()) {
            QVariant sidebarEnabledVar = hostContext_->configService()->value("video.sidebar.enabled");
            bool sidebarEnabled = (sidebarEnabledVar == true || sidebarEnabledVar.toInt() == 1
                                   || sidebarEnabledVar.toString() == "true");
            if (sidebarEnabled) {
                int sidebarW = hostContext_->configService()->value("video.sidebar.width").toInt();
                QString pos = hostContext_->configService()->value("video.sidebar.position").toString();
                if (sidebarW <= 0) sidebarW = 150;
                if (pos.isEmpty()) pos = "right";
                touchReader_->setSidebar(true, sidebarW, pos.toStdString());
                touchReader_->computeLetterbox();  // recompute with sidebar offset
                BOOST_LOG_TRIVIAL(info) << "[AAPlugin] Sidebar touch zones: "
                                        << pos.toStdString() << " " << sidebarW << "px";
            }
        }
    } else {
        BOOST_LOG_TRIVIAL(info) << "[AAPlugin] No touch device found — touch input disabled";
    }

    // Start AA service — it needs to listen for connections immediately
    aaService_->start();

    if (hostContext_)
        hostContext_->log(LogLevel::Info, QStringLiteral("Android Auto plugin initialized"));

    return true;
}

void AndroidAutoPlugin::shutdown()
{
    if (touchReader_) {
        touchReader_->requestStop();
        touchReader_->wait();
        touchReader_ = nullptr;
    }
    // AndroidAutoService cleanup happens via QObject parent deletion
    aaService_ = nullptr;
}

void AndroidAutoPlugin::onActivated(QQmlContext* context)
{
    if (!context || !aaService_) return;

    // Expose AA objects to the plugin's QML view
    context->setContextProperty("AndroidAutoService", aaService_);
    context->setContextProperty("VideoDecoder", aaService_->videoDecoder());
    context->setContextProperty("TouchHandler", aaService_->touchHandler());

    // Re-grab touch and request video focus if returning from backgrounded state
    using CS = oap::aa::AndroidAutoService;
    if (static_cast<CS::ConnectionState>(aaService_->connectionState()) == CS::Backgrounded) {
        if (touchReader_) touchReader_->grab();
        aaService_->requestVideoFocus();
        BOOST_LOG_TRIVIAL(info) << "[AAPlugin] Re-entering AA projection from background";
    }
}

void AndroidAutoPlugin::onDeactivated()
{
    // Null out the video sink BEFORE the child QML context is destroyed.
    // The ASIO threads may still be pushing frames — without this, the
    // decoder writes to a dangling QVideoSink pointer and crashes.
    if (aaService_ && aaService_->videoDecoder())
        aaService_->videoDecoder()->setVideoSink(nullptr);
}

QUrl AndroidAutoPlugin::qmlComponent() const
{
    // Points to the AA QML view in the app's resource bundle.
    // In the future this could come from the plugin's own resources.
    return QUrl(QStringLiteral("qrc:/OpenAutoProdigy/AndroidAutoMenu.qml"));
}

QUrl AndroidAutoPlugin::iconSource() const
{
    return {};  // Font-based icons — see MaterialIcon.qml (\ueff7 directions_car)
}

} // namespace plugins
} // namespace oap
