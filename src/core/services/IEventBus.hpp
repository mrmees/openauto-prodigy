#pragma once

#include <QString>
#include <QVariant>
#include <functional>

namespace oap {

/// Simple string-keyed publish/subscribe event bus.
/// Events are QVariant payloads keyed by string topics.
/// Subscribers are invoked on the main thread (Qt::QueuedConnection).
class IEventBus {
public:
    virtual ~IEventBus() = default;

    using Callback = std::function<void(const QVariant& payload)>;

    /// Subscribe to a topic. Returns a subscription ID for unsubscribe.
    /// Thread-safe.
    virtual int subscribe(const QString& topic, Callback callback) = 0;

    /// Unsubscribe by subscription ID.
    /// Thread-safe.
    virtual void unsubscribe(int subscriptionId) = 0;

    /// Publish an event. All subscribers for the topic are invoked
    /// asynchronously on the main thread.
    /// Thread-safe (can be called from any thread).
    virtual void publish(const QString& topic, const QVariant& payload = {}) = 0;
};

} // namespace oap
