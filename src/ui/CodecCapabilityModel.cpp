#include "ui/CodecCapabilityModel.hpp"
#include <QDebug>

namespace oap {

// Canonical ordering — keep H.264 first (always enabled)
static const QStringList kCodecOrder = {"h264", "h265", "vp9", "av1"};

CodecCapabilityModel::CodecCapabilityModel(QObject* parent)
    : QAbstractListModel(parent)
{
    auto caps = aa::CodecCapability::probe();

    for (const auto& name : kCodecOrder) {
        CodecEntry e;
        e.name = name;
        e.enabled = (name == "h264" || name == "h265"); // default enabled

        auto it = caps.find(name.toStdString());
        if (it != caps.end()) {
            for (const auto& d : it->second.hardware)
                e.hwDecoders.append(QString::fromStdString(d.name));
            for (const auto& d : it->second.software)
                e.swDecoders.append(QString::fromStdString(d.name));
        }

        e.hwAvailable = !e.hwDecoders.isEmpty();
        e.isHardware = false; // default to software
        e.selectedDecoder = "auto";

        entries_.append(e);
    }
}

int CodecCapabilityModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return entries_.size();
}

QVariant CodecCapabilityModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size())
        return {};

    const auto& e = entries_.at(index.row());
    switch (role) {
    case CodecNameRole:     return e.name;
    case EnabledRole:       return e.enabled;
    case HwAvailableRole:   return e.hwAvailable;
    case IsHardwareRole:    return e.isHardware;
    case DecoderListRole:   return decodersForMode(e);
    case SelectedDecoderRole: return e.selectedDecoder;
    default:                return {};
    }
}

bool CodecCapabilityModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size())
        return false;

    auto& e = entries_[index.row()];

    switch (role) {
    case EnabledRole:
        // H.264 cannot be disabled
        if (e.name == "h264")
            return false;
        e.enabled = value.toBool();
        break;

    case IsHardwareRole:
        e.isHardware = value.toBool();
        // Reset decoder to auto when switching mode
        e.selectedDecoder = "auto";
        emit dataChanged(index, index, {IsHardwareRole, DecoderListRole, SelectedDecoderRole});
        return true;

    case SelectedDecoderRole:
        e.selectedDecoder = value.toString();
        break;

    default:
        return false;
    }

    emit dataChanged(index, index, {role});
    return true;
}

Qt::ItemFlags CodecCapabilityModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

QHash<int, QByteArray> CodecCapabilityModel::roleNames() const
{
    return {
        {CodecNameRole,      "codecName"},
        {EnabledRole,        "codecEnabled"},
        {HwAvailableRole,    "hwAvailable"},
        {IsHardwareRole,     "isHardware"},
        {DecoderListRole,    "decoderList"},
        {SelectedDecoderRole, "selectedDecoder"},
    };
}

QString CodecCapabilityModel::codecName(int row) const
{
    if (row < 0 || row >= entries_.size())
        return {};
    return entries_.at(row).name;
}

bool CodecCapabilityModel::isEnabled(int row) const
{
    if (row < 0 || row >= entries_.size())
        return false;
    return entries_.at(row).enabled;
}

bool CodecCapabilityModel::isHwDecoder(int row, const QString& decoderName) const
{
    if (row < 0 || row >= entries_.size())
        return false;
    return entries_.at(row).hwDecoders.contains(decoderName);
}

void CodecCapabilityModel::setEnabled(int row, bool enabled)
{
    setData(index(row, 0), enabled, EnabledRole);
}

void CodecCapabilityModel::setHardwareMode(int row, bool hw)
{
    setData(index(row, 0), hw, IsHardwareRole);
}

void CodecCapabilityModel::setSelectedDecoder(int row, const QString& decoder)
{
    setData(index(row, 0), decoder, SelectedDecoderRole);
}

QStringList CodecCapabilityModel::decodersForMode(const CodecEntry& e) const
{
    QStringList result;
    result << "auto";
    result.append(e.isHardware ? e.hwDecoders : e.swDecoders);
    return result;
}

} // namespace oap
