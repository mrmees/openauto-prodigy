#include "PluginViewHost.hpp"
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include "../core/Logging.hpp"

namespace oap {

PluginViewHost::PluginViewHost(QQmlEngine* engine, QObject* parent)
    : QObject(parent), engine_(engine) {}

void PluginViewHost::setHostItem(QQuickItem* host)
{
    hostItem_ = host;
}

bool PluginViewHost::hasView() const
{
    return !activeView_.isNull();
}

bool PluginViewHost::loadView(const QUrl& qmlUrl, QQmlContext* pluginContext)
{
    if (!hostItem_ || !engine_ || !pluginContext) return false;

    clearView();

    QQmlComponent component(engine_, qmlUrl);
    if (component.isError()) {
        auto err = component.errorString();
        qCCritical(lcUI) << "Failed to load "
                                  << qmlUrl.toString() << ": "
                                  << err;
        emit viewLoadFailed(err);
        return false;
    }

    auto* obj = component.create(pluginContext);
    if (!obj) {
        emit viewLoadFailed("Component::create() returned null");
        return false;
    }

    auto* view = qobject_cast<QQuickItem*>(obj);
    if (!view) {
        delete obj;
        emit viewLoadFailed("Created object is not a QQuickItem");
        return false;
    }

    // PluginViewHost owns the QObject lifetime while the shell host owns the
    // visual placement.  This also lets host teardown reclaim a view whose
    // deferred-delete event has not run yet.
    view->setParent(this);
    view->setParentItem(hostItem_);
    view->setWidth(hostItem_->width());
    view->setHeight(hostItem_->height());
    activeView_ = view;

    // Each view tracks only itself.  An outgoing view waiting for deferred
    // deletion can therefore never resize or otherwise affect its replacement.
    const QPointer<QQuickItem> guardedHost = hostItem_;
    const QPointer<QQuickItem> guardedView = view;
    connect(hostItem_, &QQuickItem::widthChanged, view, [guardedHost, guardedView]() {
        if (guardedHost && guardedView)
            guardedView->setWidth(guardedHost->width());
    });
    connect(hostItem_, &QQuickItem::heightChanged, view, [guardedHost, guardedView]() {
        if (guardedHost && guardedView)
            guardedView->setHeight(guardedHost->height());
    });

    emit viewLoaded();
    return true;
}

void PluginViewHost::clearView()
{
    QPointer<QQuickItem> outgoing = activeView_;
    if (!outgoing)
        return;

    // Logical detach is immediate, while QObject destruction is deferred past
    // the input or signal dispatch which requested the transition.
    activeView_.clear();
    emit viewCleared();
    if (outgoing)
        outgoing->deleteLater();
}

} // namespace oap
