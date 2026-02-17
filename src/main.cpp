#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>
#include <QFile>
#include <memory>
#include "core/Configuration.hpp"
#include "core/aa/AndroidAutoService.hpp"
#include "ui/ThemeController.hpp"
#include "ui/ApplicationController.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("OpenAuto Prodigy");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("OpenAutoProdigy");

    auto config = std::make_shared<oap::Configuration>();

    // Load from default path if exists; otherwise use built-in defaults
    QString configPath = QDir::homePath() + "/.openauto/openauto_system.ini";
    if (QFile::exists(configPath))
        config->load(configPath);

    auto theme = new oap::ThemeController(config, &app);
    auto appController = new oap::ApplicationController(&app);
    auto aaService = new oap::aa::AndroidAutoService(config, &app);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("ThemeController", theme);
    engine.rootContext()->setContextProperty("ApplicationController", appController);
    engine.rootContext()->setContextProperty("AndroidAutoService", aaService);

    // Qt 6.5+ uses /qt/qml/ prefix, Qt 6.4 uses direct URI prefix
    QUrl url(QStringLiteral("qrc:/OpenAutoProdigy/main.qml"));
    if (QFile::exists(QStringLiteral(":/qt/qml/OpenAutoProdigy/main.qml")))
        url = QUrl(QStringLiteral("qrc:/qt/qml/OpenAutoProdigy/main.qml"));

    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    // Start AA service after QML is loaded
    aaService->start();

    return app.exec();
}
