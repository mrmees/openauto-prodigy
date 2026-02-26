#pragma once

#include "core/aa/CodecCapability.hpp"
#include <QAbstractListModel>
#include <QStringList>

namespace oap {

class CodecCapabilityModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        CodecNameRole = Qt::UserRole + 1,
        EnabledRole,
        HwAvailableRole,
        IsHardwareRole,
        DecoderListRole,
        SelectedDecoderRole,
    };

    explicit CodecCapabilityModel(QObject* parent = nullptr);

    Q_INVOKABLE QString codecName(int row) const;
    Q_INVOKABLE bool isEnabled(int row) const;
    Q_INVOKABLE bool isHwDecoder(int row, const QString& decoderName) const;
    Q_INVOKABLE void setEnabled(int row, bool enabled);
    Q_INVOKABLE void setHardwareMode(int row, bool hw);
    Q_INVOKABLE void setSelectedDecoder(int row, const QString& decoder);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct CodecEntry {
        QString name;           // "h264", "h265", "vp9", "av1"
        bool enabled;
        bool hwAvailable;
        bool isHardware;        // current mode
        QStringList hwDecoders;
        QStringList swDecoders;
        QString selectedDecoder;
    };

    QStringList decodersForMode(const CodecEntry& e) const;

    QList<CodecEntry> entries_;
};

} // namespace oap
