#pragma once

#include <QAbstractListModel>

namespace oap {

class NotificationService;

class NotificationModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles {
        NotificationIdRole = Qt::UserRole + 1,
        KindRole,
        MessageRole,
        SourcePluginRole,
        PriorityRole
    };

    explicit NotificationModel(NotificationService* service, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private:
    NotificationService* service_;
};

} // namespace oap
