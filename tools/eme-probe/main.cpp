// EME/Widevine probe: loads a qrc-served test page (secure context) that
// reports codec support and com.widevine.alpha availability to the console.
// Pass a URL argument for interactive testing (Shaka demo, Spotify, ...).
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include "core/WidevineCdm.hpp"

int main(int argc, char *argv[])
{
    const QString cdm = oap::resolveWidevineCdmPath(oap::widevineCdmCandidates());
    const QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", oap::appendWidevineFlag(flags, cdm));
    qInfo() << "eme-probe: widevine cdm =" << (cdm.isEmpty() ? QStringLiteral("NOT FOUND") : cdm);

    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);

    const QString target = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                    : QStringLiteral("qrc:/probe.html");

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("targetUrl"), target}});
    engine.load(QUrl(QStringLiteral("qrc:/probe.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
