// directoryfilter.cpp - File list filter shared by the local and the remote file list of the Project dock
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (moved out of projectdock.cpp and generalised for the remote list)

#include "directoryfilter.h"

#include "remotepaths.h"

#include <QAction>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineEdit>
#include <QMenu>
#include <QToolButton>

#include <algorithm>

DirectoryFilterProxyModel::DirectoryFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

void DirectoryFilterProxyModel::setExtensionPatterns(const QStringList& patterns, bool hasSelection)
{
    m_patterns = patterns;
    m_hasExtensionSelection = hasSelection;
    invalidate();
}

void DirectoryFilterProxyModel::setNamePattern(const QString& pattern)
{
    m_namePattern = pattern.trimmed();
    invalidate();
}

/** @brief Folders always pass (they are needed for the view root of a QFileSystemModel); files pass
 *  the search text and the extension subset. Claude Generated 2026 */
bool DirectoryFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!sourceModel())
        return false;
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;
    QString name;
    if (auto* fsModel = qobject_cast<QFileSystemModel*>(sourceModel())) {
        if (fsModel->fileInfo(index).isDir())
            return true;
        name = fsModel->fileName(index);
    } else {
        if (index.data(IsDirRole).toBool())
            return true;
        name = index.data(Qt::DisplayRole).toString();
    }
    return remotepaths::fileFilterAccepts(name, m_namePattern, m_hasExtensionSelection, m_patterns);
}

FileFilterBar::FileFilterBar(DirectoryFilterProxyModel* proxy, QWidget* parent)
    : QWidget(parent)
    , m_proxy(proxy)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Compact filter bar: search field + extension popup + clear button. No labels or large list
    // widgets to keep the dock clean.
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("Search files…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setToolTip(tr("Filter files by name (substring, case-insensitive)"));
    m_searchEdit->setMinimumWidth(200);
    layout->addWidget(m_searchEdit, 1);

    // Extension filter as a compact popup menu: all suffixes in the current directory are shown as
    // checkable actions. Checked = visible.
    m_extensionButton = new QToolButton;
    m_extensionButton->setText(tr("Extensions"));
    m_extensionButton->setPopupMode(QToolButton::InstantPopup);
    m_extensionButton->setToolTip(tr("Show/hide file extensions in the current directory"));
    m_extensionButton->setMinimumWidth(90);
    m_extensionMenu = new QMenu(m_extensionButton);
    m_extensionButton->setMenu(m_extensionMenu);
    layout->addWidget(m_extensionButton);

    m_clearButton = new QToolButton;
    m_clearButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear")));
    m_clearButton->setToolTip(tr("Clear search and extension filter"));
    m_clearButton->setAutoRaise(true);
    layout->addWidget(m_clearButton);

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (m_proxy)
            m_proxy->setNamePattern(text);
        emit filterChanged();
    });
    connect(m_clearButton, &QToolButton::clicked, this, [this]() {
        m_searchEdit->clear();
        setAllChecked(true);
    });
}

/** @brief Rebuilds the Extensions menu for the suffixes of the current directory. Claude Generated 2026 */
void FileFilterBar::setAvailableSuffixes(const QSet<QString>& suffixes)
{
    // Remember newly discovered suffixes as checked unless the user previously unchecked them.
    // Suffixes that disappear are dropped from both sets.
    for (const QString& suffix : suffixes) {
        if (!m_unchecked.contains(suffix))
            m_checked.insert(suffix);
    }
    m_checked.intersect(suffixes);
    m_unchecked.intersect(suffixes);

    m_extensionMenu->clear();
    QAction* allAction = m_extensionMenu->addAction(tr("Select all"));
    QAction* noneAction = m_extensionMenu->addAction(tr("Select none"));
    m_extensionMenu->addSeparator();
    connect(allAction, &QAction::triggered, this, [this]() { setAllChecked(true); });
    connect(noneAction, &QAction::triggered, this, [this]() { setAllChecked(false); });

    QStringList sorted = suffixes.values();
    std::sort(sorted.begin(), sorted.end());  // "" (no extension) first, as "<no extension>" sorted before
    for (const QString& suffix : sorted) {
        QAction* action = m_extensionMenu->addAction(suffix.isEmpty() ? tr("<no extension>") : suffix);
        action->setCheckable(true);
        action->setChecked(m_checked.contains(suffix));
        action->setData(suffix);
        connect(action, &QAction::toggled, this, &FileFilterBar::applyFromMenu);
    }
    applyFromMenu();
}

void FileFilterBar::applyFromMenu()
{
    QStringList patterns;
    bool hasCheckable = false;
    int total = 0;
    for (QAction* action : m_extensionMenu->actions()) {
        if (!action->isCheckable())
            continue;
        hasCheckable = true;
        ++total;
        const QString suffix = action->data().toString();
        if (action->isChecked()) {
            patterns << suffix;
            m_checked.insert(suffix);
            m_unchecked.remove(suffix);
        } else {
            m_unchecked.insert(suffix);
            m_checked.remove(suffix);
        }
    }
    if (m_proxy)
        m_proxy->setExtensionPatterns(patterns, hasCheckable);
    m_extensionButton->setText(tr("Extensions (%1/%2)").arg(patterns.size()).arg(total));
    emit filterChanged();
}

void FileFilterBar::setAllChecked(bool checked)
{
    for (QAction* action : m_extensionMenu->actions()) {
        if (action->isCheckable())
            action->setChecked(checked);  // applyFromMenu follows through the toggled signals
    }
}
