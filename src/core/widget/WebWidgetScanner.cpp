#include "core/widget/WebWidgetScanner.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>

#include "core/webwidget/WebWidgetContentResolver.hpp"
#include "core/widget/WebWidgetManifest.hpp"
#include "core/widget/WidgetRegistry.hpp"

namespace oap {

int WebWidgetScanner::scan(const QString& rootDir, WidgetRegistry& registry,
                           WebWidgetContentResolver* resolver)
{
    QDir root(rootDir);
    if (!root.exists())
        return 0;

    int count = 0;
    const auto dirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& dirName : dirs) {
        const QString manifestPath = root.filePath(dirName + QStringLiteral("/widget.yaml"));
        if (!QFile::exists(manifestPath))
            continue;
        const WebWidgetManifest m = WebWidgetManifest::fromFile(manifestPath);
        if (!m.isValid()) {
            qWarning() << "WebWidgetScanner: skipping invalid package" << manifestPath;
            continue;
        }

        WidgetDescriptor d;
        d.id = m.id;
        d.displayName = m.name;
        d.iconName = m.icon;
        d.category = m.category;
        d.description = m.description;
        d.qmlComponent = QUrl(QStringLiteral("qrc:/OpenAutoProdigy/WebWidgetHost.qml"));
        d.contributionKind = DashboardContributionKind::WebWidget;
        d.defaultConfig = QVariantMap{
            {QStringLiteral("url"),
             QStringLiteral("prodigy://widgets/%1/%2").arg(m.id, m.entry)},
        };
        d.minCols = m.minCols;
        d.minRows = m.minRows;
        d.maxCols = m.maxCols;
        d.maxRows = m.maxRows;
        d.defaultCols = m.defaultCols;
        d.defaultRows = m.defaultRows;

        if (!registry.registerWidget(d)) {
            qWarning() << "WebWidgetScanner: duplicate widget id" << m.id
                       << "— keeping the earlier registration";
            continue;
        }
        if (resolver)
            resolver->registerPackage(m.id, m.dirPath);
        ++count;
    }
    return count;
}

} // namespace oap
