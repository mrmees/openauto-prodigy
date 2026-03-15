#pragma once

#include <QObject>
#include "core/widget/WidgetTypes.hpp"

namespace oap {

class IHostContext;
class IProjectionStatusProvider;
class INavigationProvider;
class IMediaStatusProvider;

class WidgetInstanceContext : public QObject {
    Q_OBJECT
    Q_PROPERTY(int cellWidth READ cellWidth CONSTANT)
    Q_PROPERTY(int cellHeight READ cellHeight CONSTANT)
    Q_PROPERTY(QString instanceId READ instanceId CONSTANT)
    Q_PROPERTY(QString widgetId READ widgetId CONSTANT)
    Q_PROPERTY(int colSpan READ colSpan WRITE setColSpan NOTIFY colSpanChanged)
    Q_PROPERTY(int rowSpan READ rowSpan WRITE setRowSpan NOTIFY rowSpanChanged)
    Q_PROPERTY(bool isCurrentPage READ isCurrentPage WRITE setIsCurrentPage NOTIFY isCurrentPageChanged)
    Q_PROPERTY(QObject* projectionStatus READ projectionStatusObj CONSTANT)
    Q_PROPERTY(QObject* navigationProvider READ navigationProviderObj CONSTANT)
    Q_PROPERTY(QObject* mediaStatus READ mediaStatusObj CONSTANT)

public:
    WidgetInstanceContext(const GridPlacement& placement,
                          int cellWidth,
                          int cellHeight,
                          IHostContext* hostContext,
                          QObject* parent = nullptr);

    int cellWidth() const { return cellWidth_; }
    int cellHeight() const { return cellHeight_; }
    QString instanceId() const { return placement_.instanceId; }
    QString widgetId() const { return placement_.widgetId; }
    IHostContext* hostContext() const { return hostContext_; }
    const GridPlacement& placement() const { return placement_; }

    int colSpan() const { return colSpan_; }
    void setColSpan(int v);

    int rowSpan() const { return rowSpan_; }
    void setRowSpan(int v);

    bool isCurrentPage() const { return isCurrentPage_; }
    void setIsCurrentPage(bool v);

    QObject* projectionStatusObj() const;
    QObject* navigationProviderObj() const;
    QObject* mediaStatusObj() const;

signals:
    void colSpanChanged();
    void rowSpanChanged();
    void isCurrentPageChanged();

private:
    GridPlacement placement_;
    int cellWidth_;
    int cellHeight_;
    int colSpan_;
    int rowSpan_;
    bool isCurrentPage_ = false;
    IHostContext* hostContext_;
};

} // namespace oap
