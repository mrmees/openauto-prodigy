#pragma once

#include <QAbstractListModel>
#include <QPair>
#include <QVector>

namespace oap {
namespace plugins {

/// Filesystem browse model for the media player's Folders view.
/// Top level = configured roots (~/Music + mounted USB volumes); inside a
/// directory: subdirs first, then audio files, both sorted case-insensitively.
class FolderModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString breadcrumb READ breadcrumb NOTIFY pathChanged)
    Q_PROPERTY(bool atTopLevel READ atTopLevel NOTIFY pathChanged)

public:
    enum Roles { NameRole = Qt::UserRole + 1, PathRole, IsDirRole };

    explicit FolderModel(QObject* parent = nullptr);

    /// Replace the root set (label, absolute path). Resets to top level.
    void setRoots(const QVector<QPair<QString, QString>>& roots);

    Q_INVOKABLE void enter(const QString& path);
    Q_INVOKABLE bool up();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QStringList audioFilesInCurrentDir() const;

    QString breadcrumb() const;
    bool atTopLevel() const { return currentDir_.isEmpty(); }

    // QAbstractListModel
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Single source of truth for playable extensions (lowercase, no dot).
    static const QStringList& audioExtensions();

signals:
    void pathChanged();

private:
    struct Entry { QString name; QString path; bool isDir; };
    void rebuild();

    QVector<QPair<QString, QString>> roots_;  // label, path
    QString currentDir_;                      // empty = top level
    QVector<Entry> entries_;
};

} // namespace plugins
} // namespace oap
