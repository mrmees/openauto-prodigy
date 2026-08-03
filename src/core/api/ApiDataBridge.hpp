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
    struct SessionState {
        QSet<oap::data::ChannelRef> subscriptions;
        bool watchesCatalog = false;
        QString providerNamespace;
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
    void sendCatalogEvent(ApiSession* session,
                          const oap::data::Catalog& catalog);
    void fanOutCatalog();

    static oap::data::OwnerToken ownerToken(ApiSession* session);

    oap::data::DataRegistry* registry_ = nullptr;
    QHash<ApiSession*, SessionState> sessions_;
    QList<oap::data::Catalog> pendingCatalogs_;
    bool catalogFanOutActive_ = false;
};

} // namespace oap::api
