#pragma once

#include <QObject>
#include <QHash>
#include <QSet>

namespace oap {

class WidgetGridModel;
class WidgetInstanceContext;
class IHostContext;

class WidgetContextFactory : public QObject {
    Q_OBJECT
    Q_PROPERTY(int cellSide READ cellSide WRITE setCellSide NOTIFY cellSideChanged)

public:
    explicit WidgetContextFactory(WidgetGridModel* gridModel,
                                   IHostContext* hostContext,
                                   QObject* parent = nullptr);

    // Called from QML to create a WidgetInstanceContext for a delegate
    Q_INVOKABLE QObject* createContext(const QString& instanceId, QObject* parent);

    int cellSide() const { return cellSide_; }
    void setCellSide(int v);

signals:
    void cellSideChanged();

private:
    WidgetGridModel* gridModel_;
    IHostContext* hostContext_;
    int cellSide_ = 120;
    QHash<QString, QSet<WidgetInstanceContext*>> activeContexts_;
};

} // namespace oap
