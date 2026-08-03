#include "core/api/ApiDataBridge.hpp"

#include "core/api/ApiSession.hpp"

#include <QDebug>
#include <QPointer>

#include <optional>
#include <algorithm>

namespace pb = prodigy::api::v1;
namespace data = oap::data;

namespace oap::api {
namespace {

data::ProviderDefinition fromProto(const pb::DataProviderDefinition& source) {
    data::ProviderDefinition result;
    result.providerNamespace = QString::fromStdString(source.provider_namespace());
    result.displayName = QString::fromStdString(source.display_name());
    if (source.has_description())
        result.description = QString::fromStdString(source.description());
    if (source.has_provider_version())
        result.providerVersion = QString::fromStdString(source.provider_version());
    return result;
}

data::ValueType fromProto(pb::DataValueType type) {
    switch (type) {
    case pb::DATA_VALUE_TYPE_DOUBLE:           return data::ValueType::Double;
    case pb::DATA_VALUE_TYPE_SIGNED_INTEGER:   return data::ValueType::SignedInteger;
    case pb::DATA_VALUE_TYPE_UNSIGNED_INTEGER: return data::ValueType::UnsignedInteger;
    case pb::DATA_VALUE_TYPE_BOOLEAN:          return data::ValueType::Boolean;
    case pb::DATA_VALUE_TYPE_STRING:           return data::ValueType::String;
    case pb::DATA_VALUE_TYPE_ENUM:             return data::ValueType::Enum;
    case pb::DATA_VALUE_TYPE_UNSPECIFIED:       return data::ValueType::Unspecified;
    }
    return data::ValueType::Unspecified;
}

pb::DataValueType toProto(data::ValueType type) {
    switch (type) {
    case data::ValueType::Double:          return pb::DATA_VALUE_TYPE_DOUBLE;
    case data::ValueType::SignedInteger:   return pb::DATA_VALUE_TYPE_SIGNED_INTEGER;
    case data::ValueType::UnsignedInteger: return pb::DATA_VALUE_TYPE_UNSIGNED_INTEGER;
    case data::ValueType::Boolean:         return pb::DATA_VALUE_TYPE_BOOLEAN;
    case data::ValueType::String:          return pb::DATA_VALUE_TYPE_STRING;
    case data::ValueType::Enum:            return pb::DATA_VALUE_TYPE_ENUM;
    case data::ValueType::Unspecified:     return pb::DATA_VALUE_TYPE_UNSPECIFIED;
    }
    return pb::DATA_VALUE_TYPE_UNSPECIFIED;
}

data::ChannelDefinition fromProto(const pb::DataChannelDefinition& source) {
    data::ChannelDefinition result;
    result.channelName = QString::fromStdString(source.channel_name());
    result.displayName = QString::fromStdString(source.display_name());
    result.valueType = fromProto(source.value_type());
    if (source.has_unit()) result.unit = QString::fromStdString(source.unit());
    if (source.has_description())
        result.description = QString::fromStdString(source.description());
    if (source.has_nominal_interval_ms())
        result.nominalIntervalMs = source.nominal_interval_ms();
    if (source.has_stale_after_ms()) result.staleAfterMs = source.stale_after_ms();
    if (source.has_suggested_minimum())
        result.suggestedMinimum = source.suggested_minimum();
    if (source.has_suggested_maximum())
        result.suggestedMaximum = source.suggested_maximum();
    result.enumOptions.reserve(source.enum_options_size());
    for (const pb::DataEnumOption& option : source.enum_options()) {
        result.enumOptions.append(
            {option.value(), QString::fromStdString(option.label())});
    }
    return result;
}

void toProto(const data::ProviderDefinition& source,
             pb::DataProviderDefinition* target) {
    target->set_provider_namespace(source.providerNamespace.toStdString());
    target->set_display_name(source.displayName.toStdString());
    if (source.description)
        target->set_description(source.description->toStdString());
    if (source.providerVersion)
        target->set_provider_version(source.providerVersion->toStdString());
}

void toProto(const data::ChannelDefinition& source,
             pb::DataChannelDefinition* target) {
    target->set_channel_name(source.channelName.toStdString());
    target->set_display_name(source.displayName.toStdString());
    target->set_value_type(toProto(source.valueType));
    if (source.unit) target->set_unit(source.unit->toStdString());
    if (source.description)
        target->set_description(source.description->toStdString());
    if (source.nominalIntervalMs)
        target->set_nominal_interval_ms(*source.nominalIntervalMs);
    if (source.staleAfterMs) target->set_stale_after_ms(*source.staleAfterMs);
    if (source.suggestedMinimum)
        target->set_suggested_minimum(*source.suggestedMinimum);
    if (source.suggestedMaximum)
        target->set_suggested_maximum(*source.suggestedMaximum);
    for (const data::EnumOption& option : source.enumOptions) {
        pb::DataEnumOption* output = target->add_enum_options();
        output->set_value(option.value);
        output->set_label(option.label.toStdString());
    }
}

void toProto(const data::Catalog& source, pb::DataCatalog* target) {
    target->set_catalog_revision(source.revision);
    for (const data::ProviderCatalog& provider : source.providers) {
        pb::DataProviderCatalog* output = target->add_providers();
        toProto(provider.provider, output->mutable_provider());
        for (const data::ChannelDefinition& channel : provider.channels)
            toProto(channel, output->add_channels());
    }
}

std::optional<data::Quality> fromProto(pb::DataQuality quality) {
    switch (quality) {
    case pb::DATA_QUALITY_UNSPECIFIED: return data::Quality::Unknown;
    case pb::DATA_QUALITY_GOOD:        return data::Quality::Good;
    case pb::DATA_QUALITY_DEGRADED:    return data::Quality::Degraded;
    case pb::DATA_QUALITY_STALE:       return data::Quality::Stale;
    case pb::DATA_QUALITY_INVALID:     return data::Quality::Invalid;
    case pb::DATA_QUALITY_UNAVAILABLE: return data::Quality::Unavailable;
    }
    return std::nullopt;
}

std::optional<data::Scalar> fromProto(const pb::DataScalar& scalar) {
    switch (scalar.value_case()) {
    case pb::DataScalar::kDoubleValue:
        return data::Scalar(scalar.double_value());
    case pb::DataScalar::kSignedIntegerValue:
        return data::Scalar(static_cast<qint64>(scalar.signed_integer_value()));
    case pb::DataScalar::kUnsignedIntegerValue:
        return data::Scalar(static_cast<quint64>(scalar.unsigned_integer_value()));
    case pb::DataScalar::kBooleanValue:
        return data::Scalar(scalar.boolean_value());
    case pb::DataScalar::kStringValue:
        return data::Scalar(QString::fromStdString(scalar.string_value()));
    case pb::DataScalar::kEnumValue:
        return data::Scalar(data::EnumScalar{scalar.enum_value()});
    case pb::DataScalar::VALUE_NOT_SET:
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<data::Sample> fromProto(const pb::DataSample& source) {
    const std::optional<data::Quality> quality = fromProto(source.quality());
    if (!quality) return std::nullopt;

    data::Sample result;
    result.channelName = QString::fromStdString(source.channel_name());
    if (source.has_value()) result.value = fromProto(source.value());
    if (source.has_observed_at_unix_ms())
        result.observedAtUnixMs = source.observed_at_unix_ms();
    result.quality = *quality;
    return result;
}

bool isServerOnlyDataPayload(pb::ApiMessage::PayloadCase payload) {
    switch (payload) {
    case pb::ApiMessage::kRegisterDataProviderResponse:
    case pb::ApiMessage::kDeclareDataChannelsResponse:
    case pb::ApiMessage::kListDataCatalogResponse:
    case pb::ApiMessage::kSubscribeDataChannelsResponse:
    case pb::ApiMessage::kDataValuesEvent:
    case pb::ApiMessage::kDataCatalogEvent:
    case pb::ApiMessage::kDataChannelAvailabilityEvent:
        return true;
    default:
        return false;
    }
}

} // namespace

ApiDataBridge::ApiDataBridge(data::DataRegistry* registry, QObject* parent)
    : QObject(parent), registry_(registry) {
    Q_ASSERT(registry_);
    connect(registry_, &data::DataRegistry::catalogChanged,
            this, [this](quint64) { fanOutCatalog(); });
}

data::OwnerToken ApiDataBridge::ownerToken(ApiSession* session) {
    return reinterpret_cast<data::OwnerToken>(session);
}

bool ApiDataBridge::requireRequestId(ApiSession* session, quint64 requestId) {
    if (requestId != 0) return true;
    session->closeWithError(0, pb::ERROR_CODE_INVALID_REQUEST,
                            QStringLiteral("data request requires nonzero request_id"));
    return false;
}

bool ApiDataBridge::handleRequest(ApiSession* session, quint64 requestId,
                                  const PbMessage& message) {
    switch (message.payload_case()) {
    case pb::ApiMessage::kRegisterDataProviderRequest:
        if (requireRequestId(session, requestId))
            handleRegister(session, requestId, message);
        return true;
    case pb::ApiMessage::kDeclareDataChannelsRequest:
        if (requireRequestId(session, requestId))
            handleDeclare(session, requestId, message);
        return true;
    case pb::ApiMessage::kRemoveDataChannelsRequest:
        if (requireRequestId(session, requestId))
            handleRemove(session, requestId, message);
        return true;
    case pb::ApiMessage::kPublishDataValues:
        handlePublish(session, requestId, message);
        return true;
    case pb::ApiMessage::kListDataCatalogRequest:
        if (requireRequestId(session, requestId))
            handleListCatalog(session, requestId);
        return true;
    case pb::ApiMessage::kWatchDataCatalogRequest:
        if (requireRequestId(session, requestId))
            handleWatchCatalog(session, requestId, message);
        return true;
    default:
        if (!isServerOnlyDataPayload(message.payload_case())) return false;
        session->closeWithError(requestId, pb::ERROR_CODE_INVALID_REQUEST,
                                QStringLiteral("server-only data payload"));
        return true;
    }
}

void ApiDataBridge::handleRegister(ApiSession* session, quint64 requestId,
                                   const PbMessage& message) {
    const data::RegistrationResult result = registry_->registerProvider(
        ownerToken(session), fromProto(message.register_data_provider_request().provider()));
    if (result.accepted) {
        sessions_[session].providerNamespace = QString::fromStdString(
            message.register_data_provider_request().provider().provider_namespace());
    }
    PbMessage response;
    auto* payload = response.mutable_register_data_provider_response();
    payload->set_accepted(result.accepted);
    payload->set_reason(result.reason.toStdString());
    session->sendMessage(requestId, response);
}

void ApiDataBridge::handleDeclare(ApiSession* session, quint64 requestId,
                                  const PbMessage& message) {
    QList<data::ChannelDefinition> definitions;
    const auto& request = message.declare_data_channels_request();
    definitions.reserve(request.channels_size());
    for (const pb::DataChannelDefinition& definition : request.channels())
        definitions.append(fromProto(definition));

    const QList<data::DeclarationResult> results =
        registry_->declareChannels(ownerToken(session), definitions);
    PbMessage response;
    auto* payload = response.mutable_declare_data_channels_response();
    for (const data::DeclarationResult& result : results) {
        auto* output = payload->add_results();
        output->set_channel_name(result.channelName.toStdString());
        output->set_accepted(result.accepted);
        output->set_reason(result.reason.toStdString());
    }
    session->sendMessage(requestId, response);
}

void ApiDataBridge::handleRemove(ApiSession* session, quint64 requestId,
                                 const PbMessage& message) {
    QStringList names;
    for (const std::string& name :
         message.remove_data_channels_request().channel_names()) {
        names.append(QString::fromStdString(name));
    }
    registry_->removeChannels(ownerToken(session), names);
    PbMessage response;
    response.mutable_ack();
    session->sendMessage(requestId, response);
}

void ApiDataBridge::handlePublish(ApiSession* session, quint64 requestId,
                                  const PbMessage& message) {
    const QString providerNamespace =
        sessions_.value(session).providerNamespace.isEmpty()
        ? QStringLiteral("<unregistered>")
        : sessions_.value(session).providerNamespace;
    if (requestId != 0) {
        qWarning() << "API data:" << providerNamespace
                   << "dropping publication with nonzero request_id" << requestId;
        return;
    }

    QList<data::Sample> samples;
    const auto& request = message.publish_data_values();
    QHash<QString, int> lastIndices;
    for (int index = 0; index < request.samples_size(); ++index) {
        lastIndices.insert(
            QString::fromStdString(request.samples(index).channel_name()), index);
    }
    QList<int> winnerIndices = lastIndices.values();
    std::sort(winnerIndices.begin(), winnerIndices.end());
    samples.reserve(winnerIndices.size());
    for (int index : winnerIndices) {
        const pb::DataSample& source = request.samples(index);
        std::optional<data::Sample> converted = fromProto(source);
        if (converted) {
            samples.append(std::move(*converted));
        } else {
            qWarning() << "API data:" << providerNamespace
                       << "dropping malformed sample"
                       << QString::fromStdString(source.channel_name());
        }
    }

    const data::PublishResult result = registry_->publish(ownerToken(session), samples);
    for (const data::PublishDiagnostic& diagnostic : result.diagnostics) {
        qWarning() << "API data:" << providerNamespace << "dropping sample"
                   << diagnostic.channelName << diagnostic.reason;
    }
}

void ApiDataBridge::handleListCatalog(ApiSession* session, quint64 requestId) {
    PbMessage response;
    toProto(registry_->catalog(),
            response.mutable_list_data_catalog_response()->mutable_catalog());
    session->sendMessage(requestId, response);
}

void ApiDataBridge::handleWatchCatalog(ApiSession* session, quint64 requestId,
                                       const PbMessage& message) {
    const bool enabled = message.watch_data_catalog_request().enabled();
    sessions_[session].watchesCatalog = enabled;

    PbMessage response;
    response.mutable_ack();
    QPointer<ApiSession> guarded(session);
    session->sendMessage(requestId, response);
    if (enabled && guarded && guarded->state() == ApiSession::State::Ready
        && sessions_.contains(guarded.data())
        && sessions_[guarded.data()].watchesCatalog) {
        sendCatalogEvent(guarded.data(), registry_->catalog());
    }
}

void ApiDataBridge::sendCatalogEvent(ApiSession* session,
                                     const data::Catalog& catalog) {
    PbMessage event;
    toProto(catalog,
            event.mutable_data_catalog_event()->mutable_catalog());
    session->sendMessage(0, event);
}

void ApiDataBridge::fanOutCatalog() {
    pendingCatalogs_.append(registry_->catalog());
    if (catalogFanOutActive_) return;

    catalogFanOutActive_ = true;
    while (!pendingCatalogs_.isEmpty()) {
        const data::Catalog catalog = pendingCatalogs_.takeFirst();
        QList<QPointer<ApiSession>> destinations;
        destinations.reserve(sessions_.size());
        for (auto it = sessions_.cbegin(); it != sessions_.cend(); ++it) {
            if (it->watchesCatalog)
                destinations.append(QPointer<ApiSession>(it.key()));
        }

        for (const QPointer<ApiSession>& session : destinations) {
            if (!session || session->state() != ApiSession::State::Ready) continue;
            const auto state = sessions_.constFind(session.data());
            if (state == sessions_.cend() || !state->watchesCatalog) continue;
            sendCatalogEvent(session.data(), catalog);
        }
    }
    catalogFanOutActive_ = false;
}

void ApiDataBridge::sessionClosed(ApiSession* session) {
    sessions_.remove(session);
    registry_->removeOwner(ownerToken(session));
}

} // namespace oap::api
