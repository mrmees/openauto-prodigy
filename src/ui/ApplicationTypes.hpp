#pragma once

#include <QObject>
#include <QtQml/qqml.h>

namespace oap {

class ApplicationTypes : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Enum container")

public:
    enum Type {
        Launcher = 0,
        Home,
        AndroidAuto,
        Music,
        Equalizer,
        Dashboards,
        Settings,
        Telephone,
        Exit
    };
    Q_ENUM(Type)
};

} // namespace oap
