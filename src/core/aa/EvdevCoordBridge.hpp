#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include <string>
#include <vector>

#include "core/aa/TouchRouter.hpp"

namespace oap { namespace aa {

class EvdevCoordBridge : public QObject {
    Q_OBJECT
public:
    explicit EvdevCoordBridge(TouchRouter* router, QObject* parent = nullptr);

    // C++ callers with std::function callback
    void updateZone(const std::string& id, int priority,
                    float pixelX, float pixelY,
                    float pixelW, float pixelH,
                    std::function<void(int, float, float, TouchEvent)> callback);

    // QML-callable overload (QJSValue callback for future use)
    Q_INVOKABLE void updateZone(const QString& id, int priority,
                                qreal pixelX, qreal pixelY,
                                qreal pixelW, qreal pixelH);

    Q_INVOKABLE void removeZone(const QString& id);
    void removeZone(const std::string& id);

    void setDisplayMapping(int displayW, int displayH, int evdevMaxX, int evdevMaxY);

    // Accessors for testing
    int displayW() const { return displayW_; }
    int displayH() const { return displayH_; }
    int evdevMaxX() const { return evdevMaxX_; }
    int evdevMaxY() const { return evdevMaxY_; }

    float pixelToEvdevX(float px) const;
    float pixelToEvdevY(float py) const;

private:
    void rebuildAndPush();

    TouchRouter* router_;
    int displayW_ = 1024;
    int displayH_ = 600;
    int evdevMaxX_ = 4095;
    int evdevMaxY_ = 4095;

    struct ZoneEntry {
        std::string id;
        int priority;
        float px, py, pw, ph;
        std::function<void(int, float, float, TouchEvent)> cb;
    };
    std::vector<ZoneEntry> zones_;
};

}} // namespace oap::aa
