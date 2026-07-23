#pragma once

#include "IEventBus.hpp"
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QHash>
#include <QMultiHash>
#include <memory>

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
        QMutex mutex;
        QWaitCondition idle;
        bool active = true;
        int executing = 0;
    };

    QMutex mutex_;
    int nextId_ = 1;
    QHash<int, std::shared_ptr<Subscription>> subscriptions_;
    QMultiHash<QString, int> topicIndex_;
};

} // namespace oap
