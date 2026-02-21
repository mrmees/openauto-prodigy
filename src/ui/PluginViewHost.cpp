#include "PluginViewHost.hpp"
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <boost/log/trivial.hpp>

namespace oap {

PluginViewHost::PluginViewHost(QQmlEngine* engine, QObject* parent)
    : QObject(parent), engine_(engine) {}

void PluginViewHost::setHostItem(QQuickItem* host)
{
    hostItem_ = host;
}

bool PluginViewHost::loadView(const QUrl& qmlUrl, QQmlContext* pluginContext)
{
    if (!hostItem_ || !engine_ || !pluginContext) return false;

    clearView();

    QQmlComponent component(engine_, qmlUrl);
    if (component.isError()) {
        auto err = component.errorString();
        BOOST_LOG_TRIVIAL(error) << "[PluginViewHost] Failed to load "
                                  << qmlUrl.toString().toStdString() << ": "
                                  << err.toStdString();
        emit viewLoadFailed(err);
        return false;
    }

    auto* obj = component.create(pluginContext);
    if (!obj) {
        emit viewLoadFailed("Component::create() returned null");
        return false;
    }

    activeView_ = qobject_cast<QQuickItem*>(obj);
    if (!activeView_) {
        delete obj;
        emit viewLoadFailed("Created object is not a QQuickItem");
        return false;
    }

    // Parent to host and fill it
    activeView_->setParentItem(hostItem_);
    activeView_->setWidth(hostItem_->width());
    activeView_->setHeight(hostItem_->height());

    // Track host resizes
    connect(hostItem_, &QQuickItem::widthChanged, activeView_, [this]() {
        if (activeView_ && hostItem_)
            activeView_->setWidth(hostItem_->width());
    });
    connect(hostItem_, &QQuickItem::heightChanged, activeView_, [this]() {
        if (activeView_ && hostItem_)
            activeView_->setHeight(hostItem_->height());
    });

    emit viewLoaded();
    return true;
}

void PluginViewHost::clearView()
{
    if (activeView_) {
        delete activeView_;
        activeView_ = nullptr;
        emit viewCleared();
    }
}

} // namespace oap
