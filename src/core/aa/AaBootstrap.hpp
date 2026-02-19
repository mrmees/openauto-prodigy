#pragma once

#include <memory>
#include <QObject>

class QQmlApplicationEngine;

namespace oap {

class Configuration;
class ApplicationController;

namespace aa {

class AndroidAutoService;
class EvdevTouchReader;

/// Runtime state for an active AA session.
/// Holds the service and touch reader created by startAa().
struct AaRuntime {
    AndroidAutoService* service = nullptr;
    EvdevTouchReader* touchReader = nullptr;  // May be nullptr (no touch device, e.g. dev VM)
};

/// Extract all AA initialization from main.cpp into one callable function.
/// Pure extraction — NO behavior change from the monolithic version.
///
/// Creates:
/// - AndroidAutoService (using config)
/// - EvdevTouchReader if touch device exists (Pi only)
/// - Connects navigation signals (connectionStateChanged → navigateTo)
/// - Sets context properties on QML engine (AndroidAutoService, VideoDecoder, TouchHandler)
/// - Starts the AA service
///
/// @param config       Legacy INI config (still used by AA internals)
/// @param appController Navigation controller for auto-switch to AA view
/// @param engine       QML engine for setting context properties
/// @param parent       Parent QObject for memory management
/// @return AaRuntime with pointers to created objects
AaRuntime startAa(std::shared_ptr<oap::Configuration> config,
                   oap::ApplicationController* appController,
                   QQmlApplicationEngine* engine,
                   QObject* parent);

/// Clean shutdown: stop touch reader, stop AA service.
void stopAa(AaRuntime& runtime);

} // namespace aa
} // namespace oap
