#include "EventBus.hpp"
#include <QMetaObject>

namespace oap {

EventBus::EventBus(QObject* parent) : QObject(parent) {}

int EventBus::subscribe(const QString& topic, Callback callback)
{
    QMutexLocker lock(&mutex_);
    int id = nextId_++;
    subscriptions_[id] = {topic, std::move(callback)};
    topicIndex_.insert(topic, id);
    return id;
}

void EventBus::unsubscribe(int subscriptionId)
{
    QMutexLocker lock(&mutex_);
    auto it = subscriptions_.find(subscriptionId);
    if (it == subscriptions_.end()) return;
    topicIndex_.remove(it->topic, subscriptionId);
    subscriptions_.erase(it);
}

void EventBus::publish(const QString& topic, const QVariant& payload)
{
    QMutexLocker lock(&mutex_);
    auto ids = topicIndex_.values(topic);
    for (int id : ids) {
        auto it = subscriptions_.find(id);
        if (it == subscriptions_.end()) continue;
        auto cb = it->callback;  // copy callback while holding lock
        // Deliver on main thread via QueuedConnection
        QMetaObject::invokeMethod(this, [cb, payload]() {
            cb(payload);
        }, Qt::QueuedConnection);
    }
}

} // namespace oap
