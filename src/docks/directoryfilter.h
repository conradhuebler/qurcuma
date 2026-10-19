// directoryfilter.h - File list filter shared by the local and the remote file list of the Project dock
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026
//
// DirectoryFilterProxyModel (formerly private to projectdock.cpp) works on a QFileSystemModel (local
// list) or on any model whose items carry IsDirRole and the file name as display text (remote list).
// FileFilterBar is the row above a file list: search field, Extensions popup (one checkable entry
// per suffix the owner reports) and a clear button. The filter rule itself is
// remotepaths::fileFilterAccepts (tested in test_remotepaths).

#pragma once

#include <QSet>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QWidget>

class QLineEdit;
class QMenu;
class QToolButton;

class DirectoryFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    /// Item role of non-QFileSystemModel sources: true for folders (folders always pass).
    static constexpr int IsDirRole = Qt::UserRole + 100;

    explicit DirectoryFilterProxyModel(QObject* parent = nullptr);
    /// @p hasSelection false: every extension passes; true: only those in @p patterns ("" = none).
    void setExtensionPatterns(const QStringList& patterns, bool hasSelection);
    void setNamePattern(const QString& pattern);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QStringList m_patterns;
    QString m_namePattern;
    bool m_hasExtensionSelection = false;
};

class FileFilterBar : public QWidget {
    Q_OBJECT
public:
    explicit FileFilterBar(DirectoryFilterProxyModel* proxy, QWidget* parent = nullptr);
    /// Suffixes of the files now shown ("" = files without extension). Newly seen suffixes start
    /// checked unless the user unchecked them before in this session.
    void setAvailableSuffixes(const QSet<QString>& suffixes);

signals:
    /// The proxy's patterns changed; a QFileSystemModel view must re-map its root index.
    void filterChanged();

private:
    void applyFromMenu();
    void setAllChecked(bool checked);

    DirectoryFilterProxyModel* m_proxy = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QToolButton* m_extensionButton = nullptr;
    QMenu* m_extensionMenu = nullptr;
    QToolButton* m_clearButton = nullptr;
    QSet<QString> m_checked;    // session-only memory across directory changes
    QSet<QString> m_unchecked;
};
