#include "EventBus.hpp"
#include <QMetaObject>
#include <QThread>

namespace oap {

EventBus::EventBus(QObject* parent) : QObject(parent) {}

int EventBus::subscribe(const QString& topic, Callback callback)
{
    QMutexLocker lock(&mutex_);
    int id = nextId_++;
    auto subscription = std::make_shared<Subscription>();
    subscription->topic = topic;
    subscription->callback = std::move(callback);
    subscriptions_[id] = std::move(subscription);
    topicIndex_.insert(topic, id);
    return id;
}

void EventBus::unsubscribe(int subscriptionId)
{
    std::shared_ptr<Subscription> subscription;
    {
        QMutexLocker lock(&mutex_);
        auto it = subscriptions_.find(subscriptionId);
        if (it == subscriptions_.end()) return;
        subscription = it.value();
        topicIndex_.remove(subscription->topic, subscriptionId);
        subscriptions_.erase(it);
    }

    QMutexLocker stateLock(&subscription->mutex);
    subscription->active = false;
    // Callbacks execute on the bus owner thread. Waiting there would deadlock
    // when a callback unsubscribes itself; outside that thread, finality means
    // unsubscribe does not return until an in-flight callback has completed.
    if (QThread::currentThread() != thread()) {
        while (subscription->executing > 0)
            subscription->idle.wait(&subscription->mutex);
    }
}

void EventBus::publish(const QString& topic, const QVariant& payload)
{
    QList<std::shared_ptr<Subscription>> deliveries;
    {
        QMutexLocker lock(&mutex_);
        const auto ids = topicIndex_.values(topic);
        deliveries.reserve(ids.size());
        for (int id : ids) {
            auto it = subscriptions_.constFind(id);
            if (it != subscriptions_.cend())
                deliveries.append(it.value());
        }
    }

    for (const auto& subscription : deliveries) {
        // Deliver on the bus owner thread. The shared state is checked at
        // execution time so unsubscribe also cancels already-queued delivery.
        QMetaObject::invokeMethod(this, [subscription, payload]() {
            Callback callback;
            {
                QMutexLocker lock(&subscription->mutex);
                if (!subscription->active)
                    return;
                ++subscription->executing;
                callback = subscription->callback;
            }

            callback(payload);

            QMutexLocker lock(&subscription->mutex);
            --subscription->executing;
            if (subscription->executing == 0)
                subscription->idle.wakeAll();
        }, Qt::QueuedConnection);
    }
}

} // namespace oap
