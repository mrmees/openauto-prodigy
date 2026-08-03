#pragma once

// DataRegistry — main-thread live state for generic external data providers.
//
// The registry knows provider/channel identity, typed scalar compatibility,
// catalog revisions, and one retained sample per active channel. It has no
// socket, protobuf, authentication, widget, EventBus, D-Bus, OBD/CAN, or
// persistence dependency. ApiDataBridge supplies opaque session owner tokens
// and translates public protobuf messages at the boundary.

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <functional>
#include <optional>
#include <variant>

namespace oap::data {

using OwnerToken = quintptr;

enum class ValueType {
    Unspecified,
    Double,
    SignedInteger,
    UnsignedInteger,
    Boolean,
    String,
    Enum,
};

enum class Quality { Unknown, Good, Degraded, Stale, Invalid, Unavailable };

enum class UnavailableReason {
    None,
    ProviderAbsent,
    ChannelAbsent,
    ProviderDisconnected,
    ChannelRemoved,
};

struct EnumScalar {
    qint64 value = 0;
    friend bool operator==(const EnumScalar& left, const EnumScalar& right) {
        return left.value == right.value;
    }
};

using Scalar = std::variant<double, qint64, quint64, bool, QString, EnumScalar>;

struct ChannelRef {
    QString providerNamespace;
    QString channelName;

    friend bool operator==(const ChannelRef& left, const ChannelRef& right) {
        return left.providerNamespace == right.providerNamespace
            && left.channelName == right.channelName;
    }
};

size_t qHash(const ChannelRef& ref, size_t seed = 0) noexcept;

struct ProviderDefinition {
    QString providerNamespace;
    QString displayName;
    std::optional<QString> description;
    std::optional<QString> providerVersion;

    friend bool operator==(const ProviderDefinition& left,
                           const ProviderDefinition& right) {
        return left.providerNamespace == right.providerNamespace
            && left.displayName == right.displayName
            && left.description == right.description
            && left.providerVersion == right.providerVersion;
    }
};

struct EnumOption {
    qint64 value = 0;
    QString label;

    friend bool operator==(const EnumOption& left, const EnumOption& right) {
        return left.value == right.value && left.label == right.label;
    }
};

struct ChannelDefinition {
    QString channelName;
    QString displayName;
    ValueType valueType = ValueType::Unspecified;
    std::optional<QString> unit;
    std::optional<QString> description;
    std::optional<quint32> nominalIntervalMs;
    std::optional<quint32> staleAfterMs;
    std::optional<double> suggestedMinimum;
    std::optional<double> suggestedMaximum;
    QList<EnumOption> enumOptions;

    friend bool operator==(const ChannelDefinition& left,
                           const ChannelDefinition& right) {
        return left.channelName == right.channelName
            && left.displayName == right.displayName
            && left.valueType == right.valueType
            && left.unit == right.unit
            && left.description == right.description
            && left.nominalIntervalMs == right.nominalIntervalMs
            && left.staleAfterMs == right.staleAfterMs
            && left.suggestedMinimum == right.suggestedMinimum
            && left.suggestedMaximum == right.suggestedMaximum
            && left.enumOptions == right.enumOptions;
    }
};

struct Sample {
    QString channelName;
    std::optional<Scalar> value;
    std::optional<qint64> observedAtUnixMs;
    Quality quality = Quality::Unknown;
};

struct ProviderCatalog {
    ProviderDefinition provider;
    QList<ChannelDefinition> channels;
};

struct Catalog {
    quint64 revision = 0;
    QList<ProviderCatalog> providers;
};

struct RegistrationResult {
    bool accepted = false;
    QString reason;
};

struct DeclarationResult {
    QString channelName;
    bool accepted = false;
    QString reason;
};

struct PublishDiagnostic {
    QString channelName;
    QString reason;
};

struct PublishResult {
    QList<Sample> acceptedSamples;
    QList<PublishDiagnostic> diagnostics;
};

class DataRegistry final : public QObject {
    Q_OBJECT
public:
    explicit DataRegistry(QObject* parent = nullptr);

    RegistrationResult registerProvider(OwnerToken owner,
                                        const ProviderDefinition& definition);
    QList<DeclarationResult> declareChannels(
        OwnerToken owner, const QList<ChannelDefinition>& definitions);
    void removeChannels(OwnerToken owner, const QStringList& channelNames);
    PublishResult publish(OwnerToken owner, const QList<Sample>& samples);
    void removeOwner(OwnerToken owner);

    quint64 catalogRevision() const { return revision_; }
    Catalog catalog() const;
    std::optional<ChannelDefinition> definition(const ChannelRef& ref) const;
    std::optional<Sample> latestSample(const ChannelRef& ref) const;
    bool providerExists(const QString& providerNamespace) const;

    void setNowUnixMsForTest(std::function<qint64()> nowUnixMs) {
        nowUnixMs_ = std::move(nowUnixMs);
    }

signals:
    void catalogChanged(quint64 revision);
    void availabilityChanged(const oap::data::ChannelRef& ref,
                             bool available,
                             oap::data::UnavailableReason reason,
                             quint64 revision);
    void valuesAccepted(const QString& providerNamespace,
                        const QList<oap::data::Sample>& samples);

private:
    struct ChannelState {
        ChannelDefinition definition;
        std::optional<Sample> latest;
    };

    struct ProviderState {
        OwnerToken owner = 0;
        ProviderDefinition definition;
        QHash<QString, ChannelState> channels;
    };

    static bool validIdentifier(const QString& value);
    static bool scalarMatches(ValueType type, const Scalar& scalar);
    ProviderState* providerForOwner(OwnerToken owner);
    const ProviderState* providerForOwner(OwnerToken owner) const;

    QHash<QString, ProviderState> providers_;
    QHash<OwnerToken, QString> ownerNamespaces_;
    quint64 revision_ = 0;
    std::function<qint64()> nowUnixMs_;
};

} // namespace oap::data

Q_DECLARE_METATYPE(oap::data::ChannelRef)
Q_DECLARE_METATYPE(oap::data::UnavailableReason)
Q_DECLARE_METATYPE(oap::data::Sample)
Q_DECLARE_METATYPE(QList<oap::data::Sample>)
