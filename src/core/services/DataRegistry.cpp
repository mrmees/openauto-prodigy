#include "core/services/DataRegistry.hpp"

#include <QDateTime>
#include <QRegularExpression>

#include <algorithm>

namespace oap::data {

size_t qHash(const ChannelRef& ref, size_t seed) noexcept {
    seed = ::qHash(ref.providerNamespace, seed);
    return ::qHash(ref.channelName, seed);
}

DataRegistry::DataRegistry(QObject* parent)
    : QObject(parent),
      nowUnixMs_([] { return QDateTime::currentMSecsSinceEpoch(); }) {
    qRegisterMetaType<ChannelRef>();
    qRegisterMetaType<UnavailableReason>();
    qRegisterMetaType<Sample>();
    qRegisterMetaType<QList<Sample>>();
}

bool DataRegistry::validIdentifier(const QString& value) {
    static const QRegularExpression pattern(
        QStringLiteral("^[a-z0-9][a-z0-9._-]{0,127}$"));
    return pattern.match(value).hasMatch();
}

bool DataRegistry::scalarMatches(ValueType type, const Scalar& scalar) {
    switch (type) {
    case ValueType::Double:          return std::holds_alternative<double>(scalar);
    case ValueType::SignedInteger:   return std::holds_alternative<qint64>(scalar);
    case ValueType::UnsignedInteger: return std::holds_alternative<quint64>(scalar);
    case ValueType::Boolean:         return std::holds_alternative<bool>(scalar);
    case ValueType::String:          return std::holds_alternative<QString>(scalar);
    case ValueType::Enum:            return std::holds_alternative<EnumScalar>(scalar);
    case ValueType::Unspecified:     return false;
    }
    return false;
}

DataRegistry::ProviderState* DataRegistry::providerForOwner(OwnerToken owner) {
    const auto namespaceIt = ownerNamespaces_.constFind(owner);
    if (namespaceIt == ownerNamespaces_.cend()) return nullptr;
    auto providerIt = providers_.find(*namespaceIt);
    return providerIt == providers_.end() ? nullptr : &providerIt.value();
}

const DataRegistry::ProviderState* DataRegistry::providerForOwner(
    OwnerToken owner) const {
    const auto namespaceIt = ownerNamespaces_.constFind(owner);
    if (namespaceIt == ownerNamespaces_.cend()) return nullptr;
    const auto providerIt = providers_.constFind(*namespaceIt);
    return providerIt == providers_.cend() ? nullptr : &providerIt.value();
}

RegistrationResult DataRegistry::registerProvider(
    OwnerToken owner, const ProviderDefinition& definition) {
    if (!validIdentifier(definition.providerNamespace))
        return {false, QStringLiteral("invalid provider namespace")};

    const auto ownedIt = ownerNamespaces_.constFind(owner);
    if (ownedIt != ownerNamespaces_.cend()
        && *ownedIt != definition.providerNamespace) {
        return {false, QStringLiteral("session already owns a provider namespace")};
    }

    auto existingIt = providers_.find(definition.providerNamespace);
    if (existingIt != providers_.end()) {
        if (existingIt->owner != owner)
            return {false, QStringLiteral("provider namespace already active")};
        if (!(existingIt->definition == definition)) {
            existingIt->definition = definition;
            ++revision_;
            emit catalogChanged(revision_);
        }
        return {true, QString()};
    }

    ProviderState state;
    state.owner = owner;
    state.definition = definition;
    providers_.insert(definition.providerNamespace, state);
    ownerNamespaces_.insert(owner, definition.providerNamespace);
    ++revision_;
    emit catalogChanged(revision_);
    return {true, QString()};
}

QList<DeclarationResult> DataRegistry::declareChannels(
    OwnerToken owner, const QList<ChannelDefinition>& definitions) {
    QList<DeclarationResult> results;
    results.reserve(definitions.size());
    ProviderState* provider = providerForOwner(owner);
    if (!provider) {
        for (const ChannelDefinition& definition : definitions) {
            results.append({definition.channelName, false,
                            QStringLiteral("provider not registered")});
        }
        return results;
    }

    QStringList changedOrder;
    QSet<QString> changedNames;
    for (const ChannelDefinition& definition : definitions) {
        DeclarationResult result;
        result.channelName = definition.channelName;
        if (!validIdentifier(definition.channelName)) {
            result.reason = QStringLiteral("invalid channel name");
            results.append(result);
            continue;
        }
        if (definition.valueType == ValueType::Unspecified) {
            result.reason = QStringLiteral("invalid value type");
            results.append(result);
            continue;
        }

        auto channelIt = provider->channels.find(definition.channelName);
        if (channelIt != provider->channels.end()
            && channelIt->definition.valueType != definition.valueType) {
            result.reason = QStringLiteral("active channel type cannot change");
            results.append(result);
            continue;
        }

        result.accepted = true;
        results.append(result);
        if (channelIt != provider->channels.end()
            && channelIt->definition == definition) {
            continue;
        }

        if (channelIt == provider->channels.end()) {
            ChannelState state;
            state.definition = definition;
            provider->channels.insert(definition.channelName, state);
        } else {
            channelIt->definition = definition;
        }
        if (!changedNames.contains(definition.channelName)) {
            changedNames.insert(definition.channelName);
            changedOrder.append(definition.channelName);
        }
    }

    if (changedOrder.isEmpty()) return results;

    ++revision_;
    const QString providerNamespace = provider->definition.providerNamespace;
    for (const QString& channelName : changedOrder) {
        emit availabilityChanged({providerNamespace, channelName}, true,
                                 UnavailableReason::None, revision_);
    }
    emit catalogChanged(revision_);
    return results;
}

void DataRegistry::removeChannels(OwnerToken owner,
                                  const QStringList& channelNames) {
    ProviderState* provider = providerForOwner(owner);
    if (!provider) return;

    QStringList removed;
    QSet<QString> seen;
    for (const QString& channelName : channelNames) {
        if (seen.contains(channelName)) continue;
        seen.insert(channelName);
        if (provider->channels.remove(channelName) > 0)
            removed.append(channelName);
    }
    if (removed.isEmpty()) return;

    ++revision_;
    const QString providerNamespace = provider->definition.providerNamespace;
    for (const QString& channelName : removed) {
        emit availabilityChanged({providerNamespace, channelName}, false,
                                 UnavailableReason::ChannelRemoved, revision_);
    }
    emit catalogChanged(revision_);
}

PublishResult DataRegistry::publish(OwnerToken owner,
                                    const QList<Sample>& samples) {
    PublishResult result;
    ProviderState* provider = providerForOwner(owner);
    if (!provider) {
        for (const Sample& sample : samples) {
            result.diagnostics.append(
                {sample.channelName, QStringLiteral("provider not registered")});
        }
        return result;
    }

    QHash<QString, int> lastIndices;
    for (int i = 0; i < samples.size(); ++i)
        lastIndices.insert(samples[i].channelName, i);
    QList<int> winnerIndices = lastIndices.values();
    std::sort(winnerIndices.begin(), winnerIndices.end());

    for (int index : winnerIndices) {
        Sample accepted = samples[index];
        const auto channelIt = provider->channels.find(accepted.channelName);
        if (channelIt == provider->channels.end()) {
            result.diagnostics.append(
                {accepted.channelName, QStringLiteral("channel not declared")});
            continue;
        }

        const bool usable = accepted.quality == Quality::Unknown
            || accepted.quality == Quality::Good
            || accepted.quality == Quality::Degraded;
        if (usable && !accepted.value.has_value()) {
            result.diagnostics.append(
                {accepted.channelName, QStringLiteral("usable quality requires value")});
            continue;
        }
        if (accepted.value.has_value()
            && !scalarMatches(channelIt->definition.valueType, *accepted.value)) {
            result.diagnostics.append(
                {accepted.channelName, QStringLiteral("scalar type mismatch")});
            continue;
        }
        if (!accepted.observedAtUnixMs.has_value())
            accepted.observedAtUnixMs = nowUnixMs_();

        channelIt->latest = accepted;
        result.acceptedSamples.append(std::move(accepted));
    }

    if (!result.acceptedSamples.isEmpty()) {
        emit valuesAccepted(provider->definition.providerNamespace,
                            result.acceptedSamples);
    }
    return result;
}

void DataRegistry::removeOwner(OwnerToken owner) {
    const auto namespaceIt = ownerNamespaces_.find(owner);
    if (namespaceIt == ownerNamespaces_.end()) return;
    const QString providerNamespace = *namespaceIt;
    ownerNamespaces_.erase(namespaceIt);

    auto providerIt = providers_.find(providerNamespace);
    if (providerIt == providers_.end()) return;
    QStringList channelNames = providerIt->channels.keys();
    std::sort(channelNames.begin(), channelNames.end());
    providers_.erase(providerIt);

    ++revision_;
    for (const QString& channelName : channelNames) {
        emit availabilityChanged({providerNamespace, channelName}, false,
                                 UnavailableReason::ProviderDisconnected,
                                 revision_);
    }
    emit catalogChanged(revision_);
}

Catalog DataRegistry::catalog() const {
    Catalog result;
    result.revision = revision_;
    QStringList providerNamespaces = providers_.keys();
    std::sort(providerNamespaces.begin(), providerNamespaces.end());
    result.providers.reserve(providerNamespaces.size());
    for (const QString& providerNamespace : providerNamespaces) {
        const ProviderState& state = providers_[providerNamespace];
        ProviderCatalog providerCatalog;
        providerCatalog.provider = state.definition;
        QStringList channelNames = state.channels.keys();
        std::sort(channelNames.begin(), channelNames.end());
        providerCatalog.channels.reserve(channelNames.size());
        for (const QString& channelName : channelNames)
            providerCatalog.channels.append(state.channels[channelName].definition);
        result.providers.append(std::move(providerCatalog));
    }
    return result;
}

std::optional<ChannelDefinition> DataRegistry::definition(
    const ChannelRef& ref) const {
    const auto providerIt = providers_.constFind(ref.providerNamespace);
    if (providerIt == providers_.cend()) return std::nullopt;
    const auto channelIt = providerIt->channels.constFind(ref.channelName);
    if (channelIt == providerIt->channels.cend()) return std::nullopt;
    return channelIt->definition;
}

std::optional<Sample> DataRegistry::latestSample(const ChannelRef& ref) const {
    const auto providerIt = providers_.constFind(ref.providerNamespace);
    if (providerIt == providers_.cend()) return std::nullopt;
    const auto channelIt = providerIt->channels.constFind(ref.channelName);
    if (channelIt == providerIt->channels.cend()) return std::nullopt;
    return channelIt->latest;
}

bool DataRegistry::providerExists(const QString& providerNamespace) const {
    return providers_.contains(providerNamespace);
}

} // namespace oap::data
