#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("OpenAuto Prodigy");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("OpenAutoProdigy");

    QQmlApplicationEngine engine;
    engine.loadFromModule("OpenAutoProdigy", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
