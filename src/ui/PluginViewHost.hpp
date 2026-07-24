#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <functional>

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
    using RetirementCompletion = std::function<void()>;

    explicit PluginViewHost(QQmlEngine* engine, QObject* parent = nullptr);
    ~PluginViewHost() override;

    /// Set the QML host item (the container in Shell where plugin views go)
    void setHostItem(QQuickItem* host);

    /// Load a plugin's QML component into the host using the given context.
    /// Returns true on success.
    bool loadView(const QUrl& qmlUrl, QQmlContext* pluginContext);

    /// Logically detach the current view and schedule its deferred destruction.
    void clearView();

    /// clearView() with an ordered completion that runs only after the outgoing
    /// view has actually been destroyed.
    void clearViewThen(RetirementCompletion completion);

    bool hasView() const;

signals:
    void viewLoaded();
    void viewCleared();
    void viewLoadFailed(const QString& error);

private:
    friend class PluginModel;

    struct PendingRetirement {
        quint64 id;
        QPointer<QQuickItem> view;
        RetirementCompletion completion;
    };

    void finishRetirement(quint64 id);
    void finishRetirements();

    QPointer<QQmlEngine> engine_;
    QPointer<QQuickItem> hostItem_;
    QPointer<QQuickItem> activeView_;
    QList<PendingRetirement> pendingRetirements_;
    quint64 nextRetirementId_ = 1;
    bool destroying_ = false;
};

} // namespace oap
