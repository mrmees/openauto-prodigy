#pragma once

#include <QObject>
#include <QPointer>
#include <QUrl>

class QQmlEngine;
class QQmlContext;
class QQuickItem;

namespace oap {

/// Manages plugin QML view instantiation with the correct child context.
/// Shell provides a host QQuickItem; PluginViewHost creates/destroys
/// plugin views as children of that host.
class PluginViewHost : public QObject {
    Q_OBJECT
public:
    explicit PluginViewHost(QQmlEngine* engine, QObject* parent = nullptr);

    /// Set the QML host item (the container in Shell where plugin views go)
    void setHostItem(QQuickItem* host);

    /// Load a plugin's QML component into the host using the given context.
    /// Returns true on success.
    bool loadView(const QUrl& qmlUrl, QQmlContext* pluginContext);

    /// Logically detach the current view and schedule its deferred destruction.
    void clearView();

    bool hasView() const;

signals:
    void viewLoaded();
    void viewCleared();
    void viewLoadFailed(const QString& error);

private:
    QPointer<QQmlEngine> engine_;
    QPointer<QQuickItem> hostItem_;
    QPointer<QQuickItem> activeView_;
};

} // namespace oap
