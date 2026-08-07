#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

namespace oap {

// Maps prodigy://widgets/<id>/<path> to files inside registered package
// directories. Pure logic (no WebEngine types) so it tests headless;
// WebWidgetSchemeHandler is the thin WebEngine shell around it (design §3).
class WebWidgetContentResolver {
public:
    void registerPackage(const QString& id, const QString& dirPath);
    void setDataRoot(const QString& dirPath);

    // Absolute canonical file path, or empty for unknown id, missing file,
    // or any path escaping the package dir (traversal / symlink).
    QString resolve(const QString& id, const QString& relativePath) const;

    static QByteArray contentTypeFor(const QString& filePath);

private:
    QString resolveWidgetData(const QString& id, const QString& relativePath) const;

    QHash<QString, QString> packages_;  // id -> canonical package dir
    QString dataRoot_;                  // cleaned absolute path; may appear later
};

} // namespace oap
