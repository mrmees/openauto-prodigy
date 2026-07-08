#include "WidevineCdm.hpp"

#include <QFileInfo>

namespace oap {

QStringList widevineCdmCandidates()
{
    return {
        QStringLiteral("/opt/WidevineCdm/gmp-widevinecdm/latest/libwidevinecdm.so"),
        QStringLiteral("/opt/WidevineCdm/_platform_specific/linux_arm64/libwidevinecdm.so"),
    };
}

QString resolveWidevineCdmPath(const QStringList& candidates)
{
    for (const QString& path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

QByteArray appendWidevineFlag(const QByteArray& existingFlags, const QString& cdmPath)
{
    if (cdmPath.isEmpty() || existingFlags.contains("widevine-path"))
        return existingFlags;

    QByteArray flag = "--widevine-path=" + cdmPath.toUtf8();
    if (existingFlags.isEmpty())
        return flag;
    return existingFlags + ' ' + flag;
}

} // namespace oap
