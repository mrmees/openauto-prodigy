#include "FolderModel.hpp"

#include <QDir>
#include <QFileInfo>

namespace oap {
namespace plugins {

FolderModel::FolderModel(QObject* parent) : QAbstractListModel(parent) {}

const QStringList& FolderModel::audioExtensions() {
    static const QStringList exts = {
        QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("ogg"),
        QStringLiteral("opus"), QStringLiteral("m4a"), QStringLiteral("aac"),
        QStringLiteral("wav"),
    };
    return exts;
}

void FolderModel::setRoots(const QVector<QPair<QString, QString>>& roots) {
    roots_ = roots;
    currentDir_.clear();
    rebuild();
    emit pathChanged();
}

void FolderModel::enter(const QString& path) {
    if (!QFileInfo(path).isDir()) return;
    currentDir_ = path;
    rebuild();
    emit pathChanged();
}

bool FolderModel::up() {
    if (atTopLevel()) return false;
    // If the current dir IS one of the roots, go back to the root list.
    for (const auto& r : roots_) {
        if (QDir::cleanPath(currentDir_) == QDir::cleanPath(r.second)) {
            currentDir_.clear();
            rebuild();
            emit pathChanged();
            return true;
        }
    }
    QDir d(currentDir_);
    if (!d.cdUp()) return false;
    currentDir_ = d.absolutePath();
    rebuild();
    emit pathChanged();
    return true;
}

void FolderModel::refresh() {
    rebuild();
}

QStringList FolderModel::audioFilesInCurrentDir() const {
    QStringList files;
    for (const auto& e : entries_)
        if (!e.isDir) files << e.path;
    return files;
}

QString FolderModel::breadcrumb() const {
    if (atTopLevel()) return QStringLiteral("Sources");
    // Show "<RootLabel> / relative / path" when under a known root.
    for (const auto& r : roots_) {
        const QString rootPath = QDir::cleanPath(r.second);
        const QString cur = QDir::cleanPath(currentDir_);
        if (cur == rootPath) return r.first;
        if (cur.startsWith(rootPath + QLatin1Char('/')))
            return r.first + QStringLiteral(" / ")
                 + cur.mid(rootPath.size() + 1).replace(QLatin1Char('/'), QStringLiteral(" / "));
    }
    return QFileInfo(currentDir_).fileName();
}

void FolderModel::rebuild() {
    beginResetModel();
    entries_.clear();
    if (atTopLevel()) {
        for (const auto& r : roots_)
            if (QFileInfo(r.second).isDir())
                entries_.append({r.first, r.second, true});
    } else {
        QDir dir(currentDir_);
        QStringList nameFilters;
        for (const QString& ext : audioExtensions())
            nameFilters << QStringLiteral("*.") + ext;
        const auto dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                            QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& fi : dirs)
            entries_.append({fi.fileName(), fi.absoluteFilePath(), true});
        const auto files = dir.entryInfoList(nameFilters, QDir::Files,
                                             QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& fi : files)
            entries_.append({fi.fileName(), fi.absoluteFilePath(), false});
    }
    endResetModel();
}

int FolderModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : entries_.size();
}

QVariant FolderModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= entries_.size()) return {};
    const Entry& e = entries_.at(index.row());
    switch (role) {
    case NameRole: return e.name;
    case PathRole: return e.path;
    case IsDirRole: return e.isDir;
    default: return {};
    }
}

QHash<int, QByteArray> FolderModel::roleNames() const {
    return {{NameRole, "name"}, {PathRole, "path"}, {IsDirRole, "isDir"}};
}

} // namespace plugins
} // namespace oap
