#pragma once

// ApiDataBridge — protobuf/session boundary for the generic live-data API.
// DataRegistry remains transport-agnostic; this class validates wire messages,
// owns per-session consumer state, and serializes direct request/event replies.
// Main thread only.

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>

#include "api/api.pb.h"
#include "core/services/DataRegistry.hpp"

#include <optional>

namespace oap::api {

class ApiSession;

class ApiDataBridge final : public QObject {
    Q_OBJECT
public:
    explicit ApiDataBridge(oap::data::DataRegistry* registry,
                           QObject* parent = nullptr);

    bool handleRequest(ApiSession* session, quint64 requestId,
                       const prodigy::api::v1::ApiMessage& message);
    void sessionClosed(ApiSession* session);

private:
    struct AvailabilityBoundary {
        bool available = false;
        oap::data::UnavailableReason reason =
            oap::data::UnavailableReason::ProviderAbsent;
        std::optional<oap::data::ChannelDefinition> definition;

        friend bool operator==(const AvailabilityBoundary& left,
                               const AvailabilityBoundary& right) {
            return left.available == right.available
                && left.reason == right.reason
                && left.definition == right.definition;
        }
    };

    struct SessionState {
        QSet<oap::data::ChannelRef> subscriptions;
        QHash<oap::data::ChannelRef, AvailabilityBoundary> lastAvailability;
        bool watchesCatalog = false;
        QString providerNamespace;
    };

    struct AvailabilityWork {
        oap::data::ChannelRef ref;
        AvailabilityBoundary boundary;
        quint64 revision = 0;
    };

    using PbMessage = prodigy::api::v1::ApiMessage;

    bool requireRequestId(ApiSession* session, quint64 requestId);
    void handleRegister(ApiSession* session, quint64 requestId,
                        const PbMessage& message);
    void handleDeclare(ApiSession* session, quint64 requestId,
                       const PbMessage& message);
    void handleRemove(ApiSession* session, quint64 requestId,
                      const PbMessage& message);
    void handlePublish(ApiSession* session, quint64 requestId,
                       const PbMessage& message);
    void handleListCatalog(ApiSession* session, quint64 requestId);
    void handleWatchCatalog(ApiSession* session, quint64 requestId,
                            const PbMessage& message);
    void handleSubscribe(ApiSession* session, quint64 requestId,
                         const PbMessage& message);
    void handleUnsubscribe(ApiSession* session, quint64 requestId,
                           const PbMessage& message);
    void sendCatalogEvent(ApiSession* session,
                          const oap::data::Catalog& catalog);
    void fanOutCatalog(const oap::data::Catalog& catalog);
    void sendAvailability(ApiSession* session,
                          const oap::data::ChannelRef& ref,
                          const AvailabilityBoundary& boundary,
                          quint64 revision);
    void sendValues(ApiSession* session, const QString& providerNamespace,
                    const QList<oap::data::Sample>& samples);
    AvailabilityBoundary currentBoundary(
        const oap::data::ChannelRef& ref) const;
    void reconcileAvailability();
    void fanOutAvailability(const oap::data::ChannelRef& ref,
                            const AvailabilityBoundary& boundary,
                            quint64 revision);
    void fanOutValues(const QString& providerNamespace,
                      const QList<oap::data::Sample>& samples);

    static oap::data::OwnerToken ownerToken(ApiSession* session);

    oap::data::DataRegistry* registry_ = nullptr;
    QHash<ApiSession*, SessionState> sessions_;
    QList<oap::data::Catalog> pendingCatalogs_;
    bool catalogFanOutActive_ = false;
    QList<AvailabilityWork> pendingAvailability_;
    bool availabilityFanOutActive_ = false;
};

} // namespace oap::api
