#pragma once

#include <QObject>
#include "core/widget/WidgetTypes.hpp"
#include <optional>

namespace oap {

class WidgetRegistry;

class WidgetPlacementModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString activePageId READ activePageId WRITE setActivePageId NOTIFY activePageIdChanged)

public:
    explicit WidgetPlacementModel(WidgetRegistry* registry, QObject* parent = nullptr);

    QString activePageId() const { return activePageId_; }
    void setActivePageId(const QString& pageId);

    void setPlacements(const QList<WidgetPlacement>& placements);
    QList<WidgetPlacement> allPlacements() const { return placements_; }

    Q_INVOKABLE QUrl qmlComponentForPane(const QString& paneId) const;
    Q_INVOKABLE void swapWidget(const QString& paneId, const QString& widgetId);
    Q_INVOKABLE void clearPane(const QString& paneId);

    std::optional<WidgetPlacement> placementForPane(const QString& paneId) const;

signals:
    void activePageIdChanged();
    void placementsChanged();
    void paneChanged(const QString& paneId);

private:
    WidgetRegistry* registry_;
    QList<WidgetPlacement> placements_;
    QString activePageId_ = QStringLiteral("home");
};

} // namespace oap
