#include "ProjectedDisplayConfig.hpp"

#include "../YamlConfig.hpp"

#include <QMetaType>
#include <QSet>

#include <cmath>
#include <limits>

#include <QString>
#include <QVariant>

namespace oap::aa {

namespace {

bool readInteger(const QVariant& value, int* result)
{
    if (!result || value.metaType().id() == QMetaType::Bool
        || value.metaType().id() == QMetaType::QString) {
        return false;
    }

    bool ok = false;
    const double number = value.toDouble(&ok);
    if (!ok || !std::isfinite(number) || std::trunc(number) != number
        || number < static_cast<double>(std::numeric_limits<int>::min())
        || number > static_cast<double>(std::numeric_limits<int>::max())) {
        return false;
    }
    *result = static_cast<int>(number);
    return true;
}

} // namespace

QString GalVersion::toString() const
{
    return QStringLiteral("%1.%2").arg(major).arg(minor);
}

ProjectedViewportGeometry ProjectedClusterProfile::geometry() const
{
    const bool is720p = resolution == QStringLiteral("720p");
    return {is720p ? 1280 : 800,
            is720p ? 720 : 480,
            contentWidth,
            contentHeight};
}

bool ProjectedClusterProfile::operator==(
    const ProjectedClusterProfile& other) const
{
    return resolution == other.resolution && dpi == other.dpi
        && contentWidth == other.contentWidth
        && contentHeight == other.contentHeight
        && turnDataAvailable == other.turnDataAvailable
        && galVersion == other.galVersion;
}

bool applyProjectedClusterProfileUpdate(
    const ProjectedClusterProfile& current,
    const QVariantMap& update,
    ProjectedClusterProfile* result,
    QString* error)
{
    auto reject = [error](const QString& message) {
        if (error)
            *error = message;
        return false;
    };
    if (!result)
        return reject(QStringLiteral("Missing profile output"));
    if (update.isEmpty())
        return reject(QStringLiteral("Profile update is empty"));

    static const QSet<QString> allowedKeys{
        QStringLiteral("resolution"),
        QStringLiteral("dpi"),
        QStringLiteral("content_width"),
        QStringLiteral("content_height"),
        QStringLiteral("turn_data_available"),
        QStringLiteral("gal_version"),
    };
    for (auto it = update.cbegin(); it != update.cend(); ++it) {
        if (!allowedKeys.contains(it.key()))
            return reject(QStringLiteral("Unknown profile key: %1").arg(it.key()));
    }

    ProjectedClusterProfile candidate = current;
    if (update.contains(QStringLiteral("gal_version"))) {
        const QVariant value = update.value(QStringLiteral("gal_version"));
        if (value.metaType().id() != QMetaType::QString)
            return reject(QStringLiteral("gal_version must be a string"));
        const QString version = value.toString().trimmed();
        if (version == QStringLiteral("1.7")) {
            candidate.galVersion = kGalVersion1_7;
        } else if (version == QStringLiteral("4.3")) {
            candidate.galVersion = kGalVersion4_3;
        } else {
            return reject(QStringLiteral("gal_version must be 1.7 or 4.3"));
        }
    }
    if (candidate.galVersion != kGalVersion1_7
        && candidate.galVersion != kGalVersion4_3) {
        return reject(QStringLiteral("gal_version must be 1.7 or 4.3"));
    }
    if (update.contains(QStringLiteral("resolution"))) {
        const QVariant value = update.value(QStringLiteral("resolution"));
        if (value.metaType().id() != QMetaType::QString)
            return reject(QStringLiteral("resolution must be a string"));
        candidate.resolution = value.toString().toLower();
    }
    if (candidate.resolution != QStringLiteral("480p")
        && candidate.resolution != QStringLiteral("720p")) {
        return reject(QStringLiteral("resolution must be 480p or 720p"));
    }

    auto applyInteger = [&](const char* key, int* target) {
        const QString name = QString::fromLatin1(key);
        if (!update.contains(name))
            return true;
        if (!readInteger(update.value(name), target)) {
            if (error)
                *error = QStringLiteral("%1 must be an integer").arg(name);
            return false;
        }
        return true;
    };
    if (!applyInteger("dpi", &candidate.dpi)
        || !applyInteger("content_width", &candidate.contentWidth)
        || !applyInteger("content_height", &candidate.contentHeight)) {
        return false;
    }

    if (update.contains(QStringLiteral("turn_data_available"))) {
        const QVariant value = update.value(QStringLiteral("turn_data_available"));
        if (value.metaType().id() != QMetaType::Bool)
            return reject(QStringLiteral("turn_data_available must be boolean"));
        candidate.turnDataAvailable = value.toBool();
    }
    if (candidate.turnDataAvailable
        && candidate.galVersion == kGalVersion1_7) {
        return reject(QStringLiteral(
            "turn_data_available requires gal_version 4.3"));
    }

    if (candidate.dpi < 80 || candidate.dpi > 640)
        return reject(QStringLiteral("dpi must be between 80 and 640"));
    if (!candidate.geometry().isValid()) {
        return reject(QStringLiteral(
            "content dimensions must fit the carrier with even centered margins"));
    }

    *result = candidate;
    if (error)
        error->clear();
    return true;
}

ProjectedClusterConfig resolveProjectedClusterConfig(const oap::YamlConfig& config)
{
    static const QString pluginId = QStringLiteral("org.openauto.android-auto");

    ProjectedClusterConfig resolved;
    resolved.enabled = config.pluginValue(
        pluginId, QStringLiteral("experimental_cluster_display")).toBool();

    const QString focus = config.pluginValue(
        pluginId, QStringLiteral("experimental_cluster_setup_focus"))
                              .toString()
                              .trimmed()
                              .toLower();
    if (focus == QStringLiteral("projected"))
        resolved.setupFocus = ProjectedSetupFocus::Projected;

    return resolved;
}

} // namespace oap::aa
