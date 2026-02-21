#pragma once

#include "IEventBus.hpp"
#include <QObject>
#include <QMutex>
#include <QHash>
#include <QMultiHash>

namespace oap {

class EventBus : public QObject, public IEventBus {
    Q_OBJECT
public:
    explicit EventBus(QObject* parent = nullptr);

    int subscribe(const QString& topic, Callback callback) override;
    void unsubscribe(int subscriptionId) override;
    void publish(const QString& topic, const QVariant& payload = {}) override;

private:
    struct Subscription {
        QString topic;
        Callback callback;
    };

    QMutex mutex_;
    int nextId_ = 1;
    QHash<int, Subscription> subscriptions_;
    QMultiHash<QString, int> topicIndex_;
};

} // namespace oap
