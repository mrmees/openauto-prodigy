#include "AndroidAutoRuntimeBridge.hpp"
#include "AndroidAutoOrchestrator.hpp"
#include "EvdevTouchReader.hpp"
#include "EvdevCoordBridge.hpp"
#include "ServiceDiscoveryBuilder.hpp"
#include "core/InputDeviceScanner.hpp"
#include "core/services/IConfigService.hpp"
#include "ui/DisplayInfo.hpp"
#include "core/Logging.hpp"
#include <QFile>
#include <cmath>
#include <tuple>

namespace oap {
namespace aa {

AndroidAutoRuntimeBridge::AndroidAutoRuntimeBridge(QObject* parent)
    : QObject(parent)
{
}

AndroidAutoRuntimeBridge::~AndroidAutoRuntimeBridge()
{
    shutdown();
}

int AndroidAutoRuntimeBridge::computeNavbarThickness(int dpi, double globalScale)
{
    if (dpi <= 0)
        return 56;
    double dpiScale = static_cast<double>(dpi) / 160.0;
    return static_cast<int>(std::round(56.0 * dpiScale * globalScale));
}

std::pair<int, int> AndroidAutoRuntimeBridge::resolveAAResolution(IConfigService* configService)
{
    int aaW = 1280, aaH = 720;
    if (configService) {
        QString res = configService->value("video.resolution").toString();
        if (res == QLatin1String("1080p")) { aaW = 1920; aaH = 1080; }
        else if (res == QLatin1String("480p")) { aaW = 800; aaH = 480; }
    }
    return {aaW, aaH};
}

QString AndroidAutoRuntimeBridge::resolveTouchDevice(IConfigService* configService) const
{
    QString touchDevice;
    if (configService) {
        touchDevice = configService->value("touch.device").toString();
    }
    if (touchDevice.isEmpty()) {
        touchDevice = oap::InputDeviceScanner::findTouchDevice();
        if (!touchDevice.isEmpty()) {
            qCInfo(lcAA) << "Auto-detected touch device:" << touchDevice;
        }
    }
    return touchDevice;
}

void AndroidAutoRuntimeBridge::setup(AndroidAutoOrchestrator* orchestrator,
                                      DisplayInfo* displayInfo,
                                      IConfigService* configService)
{
    displayInfo_ = displayInfo;
    configService_ = configService;

    // Read display dimensions
    if (displayInfo && displayInfo->windowWidth() > 0 && displayInfo->windowHeight() > 0) {
        displayW_ = displayInfo->windowWidth();
        displayH_ = displayInfo->windowHeight();
        qCInfo(lcAA) << "Display dimensions: detected" << displayW_ << "x" << displayH_;
    } else {
        qCInfo(lcAA) << "Display dimensions: using defaults" << displayW_ << "x" << displayH_;
    }

    // Compute DPI-scaled navbar thickness
    navbarThick_ = 56;
    if (displayInfo && displayInfo->computedDpi() > 0) {
        double globalScale = 1.0;
        if (configService) {
            QVariant gs = configService->value("ui.scale");
            if (!gs.isNull() && gs.toDouble() > 0) globalScale = gs.toDouble();
        }
        navbarThick_ = computeNavbarThickness(displayInfo->computedDpi(), globalScale);
    }
    qCInfo(lcAA) << "Navbar thickness (DPI-scaled):" << navbarThick_ << "px";

    // Inject display dimensions + navbar thickness into orchestrator
    if (orchestrator) {
        if (displayInfo && displayInfo->windowWidth() > 0 && displayInfo->windowHeight() > 0) {
            orchestrator->setDisplayDimensions(displayW_, displayH_);
        }
        orchestrator->setNavbarThickness(navbarThick_);
    }

    // Resolve AA video resolution
    auto [aaW, aaH] = resolveAAResolution(configService);

    // Resolve touch device
    QString touchDevice = resolveTouchDevice(configService);

    if (!touchDevice.isEmpty() && QFile::exists(touchDevice) && orchestrator) {
        touchReader_ = new EvdevTouchReader(
            orchestrator->touchHandler(),
            touchDevice.toStdString(),
            aaW, aaH,
            displayW_, displayH_,
            this);

        // Create EvdevCoordBridge for external zone registration
        coordBridge_ = new EvdevCoordBridge(&touchReader_->router(), this);
        coordBridge_->setDisplayMapping(displayW_, displayH_, 4095, 4095);

        // Relay 3-finger gesture
        connect(touchReader_, &EvdevTouchReader::gestureDetected,
                this, &AndroidAutoRuntimeBridge::gestureTriggered,
                Qt::QueuedConnection);

        updateVideoMapping(aaW, aaH);
        touchReader_->start();
        qCInfo(lcAA) << "Touch:" << touchDevice
                << "display=" << displayW_ << "x" << displayH_;
    } else {
        // Keep the observable mapping aligned even when this installation has
        // no evdev reader (for example desktop development).
        updateVideoMapping(aaW, aaH);
        qCInfo(lcAA) << "No touch device found — touch input disabled";
    }

    // Wire dynamic display dimension updates
    if (displayInfo) {
        connect(displayInfo, &DisplayInfo::windowSizeChanged, this, [this, orchestrator]() {
            int w = displayInfo_->windowWidth();
            int h = displayInfo_->windowHeight();
            if (w > 0 && h > 0) {
                displayW_ = w;
                displayH_ = h;
                double globalScale = 1.0;
                if (configService_) {
                    const QVariant scale = configService_->value("ui.scale");
                    if (!scale.isNull() && scale.toDouble() > 0)
                        globalScale = scale.toDouble();
                }
                navbarThick_ = computeNavbarThickness(
                    displayInfo_->computedDpi(), globalScale);
                if (touchReader_)
                    touchReader_->setDisplayDimensions(w, h);
                if (coordBridge_)
                    coordBridge_->setDisplayMapping(w, h, 4095, 4095);
                if (orchestrator) {
                    orchestrator->setDisplayDimensions(w, h);
                    orchestrator->setNavbarThickness(navbarThick_);
                }
                const auto [aaW, aaH] = resolveAAResolution(configService_);
                updateVideoMapping(aaW, aaH);
            }
        });
    }
}

void AndroidAutoRuntimeBridge::updateVideoMapping(int aaWidth, int aaHeight)
{
    bool navbarDuringAA = true;
    QString navEdge = QStringLiteral("bottom");
    if (configService_) {
        const QVariant showDuringAA = configService_->value("navbar.show_during_aa");
        navbarDuringAA = showDuringAA.isNull() || showDuringAA.toBool();
        const QString configuredEdge = configService_->value("navbar.edge").toString();
        if (!configuredEdge.isEmpty())
            navEdge = configuredEdge;
    }

    std::tie(contentW_, contentH_) = ServiceDiscoveryBuilder::computeContentDimensions(
        aaWidth, aaHeight, displayW_, displayH_, navbarDuringAA, navEdge, navbarThick_);

    if (touchReader_) {
        touchReader_->setNavbar(navbarDuringAA, navbarThick_, navEdge.toStdString());
        touchReader_->setVideoMapping(aaWidth, aaHeight, contentW_, contentH_);
    }

    qCInfo(lcAA) << "Content dims updated:" << contentW_ << "x" << contentH_
                 << "(video:" << aaWidth << "x" << aaHeight
                 << "display:" << displayW_ << "x" << displayH_ << ")";
}

void AndroidAutoRuntimeBridge::shutdown()
{
    if (touchReader_) {
        touchReader_->requestStop();
        touchReader_->wait();
        touchReader_ = nullptr;
    }
    coordBridge_ = nullptr;
}

void AndroidAutoRuntimeBridge::grab()
{
    if (touchReader_) touchReader_->grab();
}

void AndroidAutoRuntimeBridge::ungrab()
{
    if (touchReader_) touchReader_->ungrab();
}

} // namespace aa
} // namespace oap
