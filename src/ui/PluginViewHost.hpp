#pragma once

#include <QObject>
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

    /// Destroy the current plugin view (must be called before context deactivation).
    void clearView();

    bool hasView() const { return activeView_ != nullptr; }

signals:
    void viewLoaded();
    void viewCleared();
    void viewLoadFailed(const QString& error);

private:
    QQmlEngine* engine_;
    QQuickItem* hostItem_ = nullptr;
    QQuickItem* activeView_ = nullptr;
};

} // namespace oap
