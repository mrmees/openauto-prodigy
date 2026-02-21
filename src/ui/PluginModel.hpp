#pragma once

#include <QAbstractListModel>
#include <QString>

class QQmlEngine;

namespace oap {

class PluginManager;
class PluginRuntimeContext;
class IPlugin;

/// QAbstractListModel exposing loaded plugins to QML for the nav strip.
/// Backed by PluginManager::plugins(). Manages PluginRuntimeContext lifecycle.
class PluginModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString activePluginId READ activePluginId NOTIFY activePluginChanged)
    Q_PROPERTY(QUrl activePluginQml READ activePluginQml NOTIFY activePluginChanged)
    Q_PROPERTY(bool activePluginFullscreen READ activePluginFullscreen NOTIFY activePluginChanged)

public:
    enum Roles {
        PluginIdRole = Qt::UserRole + 1,
        PluginNameRole,
        PluginIconRole,
        PluginQmlRole,
        IsActiveRole,
        WantsFullscreenRole
    };

    explicit PluginModel(PluginManager* manager, QQmlEngine* engine, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString activePluginId() const;
    QUrl activePluginQml() const;
    bool activePluginFullscreen() const;

    Q_INVOKABLE void setActivePlugin(const QString& pluginId);

signals:
    void activePluginChanged();

private:
    PluginManager* manager_;
    QQmlEngine* engine_;
    PluginRuntimeContext* activeContext_ = nullptr;
    QString activePluginId_;

    IPlugin* activePlugin() const;
};

} // namespace oap
