// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// BookmarkWidget implementation.
//
// Claude Generated 2026 - Dock system restructuring.

#include "bookmarkwidget.h"

#include <QDateTime>
#include <QDir>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

BookmarkWidget::BookmarkWidget(Settings* settings, QWidget* parent)
    : QWidget(parent)
    , m_settings(settings)
{
    setupUI();
    buildTree();
}

void BookmarkWidget::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    QWidget* header = new QWidget;
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* label = new QLabel(tr("Bookmarks"));
    label->setStyleSheet(QStringLiteral("font-weight: bold;"));
    headerLayout->addWidget(label, 1);

    m_bookmarkButton = new QToolButton;
    m_bookmarkButton->setIcon(QIcon::fromTheme(QStringLiteral("bookmark-new"),
                                              QIcon(QStringLiteral(":/icons/bookmark.png"))));
    m_bookmarkButton->setToolTip(tr("Bookmark current directory"));
    connect(m_bookmarkButton, &QToolButton::clicked,
            this, &BookmarkWidget::onBookmarkCurrentDirectory);
    headerLayout->addWidget(m_bookmarkButton);
    layout->addWidget(header);

    m_treeView = new QTreeWidget;
    m_treeView->setHeaderLabels(QStringList() << tr("Bookmarks"));
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setDragDropMode(QAbstractItemView::InternalMove);
    connect(m_treeView, &QTreeWidget::itemClicked,
            this, &BookmarkWidget::onItemClicked);
    connect(m_treeView, &QTreeWidget::customContextMenuRequested,
            this, &BookmarkWidget::onContextMenu);
    layout->addWidget(m_treeView);
}

void BookmarkWidget::refresh()
{
    buildTree();
}

QTreeWidget* BookmarkWidget::treeView() const
{
    return m_treeView;
}

QString BookmarkWidget::selectedPath() const
{
    QTreeWidgetItem* item = m_treeView ? m_treeView->currentItem() : nullptr;
    if (!item)
        return QString();
    return item->data(0, Qt::UserRole + 1).toString();
}

void BookmarkWidget::buildTree()
{
    if (!m_treeView || !m_settings)
        return;

    m_treeView->clear();
    QVector<Settings::BookmarkItem> allBookmarks = m_settings->bookmarks();

    QMap<QString, QTreeWidgetItem*> itemsById;
    itemsById[""] = m_treeView->invisibleRootItem();

    for (const auto& bm : allBookmarks) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, bm.name);
        item->setData(0, Qt::UserRole, bm.id);
        item->setData(0, Qt::UserRole + 1, bm.path);

        if (bm.isFolder) {
            item->setIcon(0, QIcon::fromTheme(QStringLiteral("folder"),
                                              QIcon(QStringLiteral(":/icons/folder.png"))));
        } else {
            item->setIcon(0, QIcon::fromTheme(QStringLiteral("bookmark"),
                                              QIcon(QStringLiteral(":/icons/bookmark.png"))));
            QString tooltip = bm.path;
            if (!bm.tags.isEmpty())
                tooltip += QStringLiteral("\nTags: ") + bm.tags.join(QStringLiteral(", "));
            item->setToolTip(0, tooltip);
        }

        if (bm.color.isValid())
            item->setBackground(0, bm.color);

        itemsById[bm.id] = item;
    }

    for (const auto& bm : allBookmarks) {
        if (itemsById.contains(bm.id)) {
            QTreeWidgetItem* parentItem = itemsById.value(bm.parentId,
                m_treeView->invisibleRootItem());
            if (parentItem && parentItem != itemsById[bm.id])
                parentItem->addChild(itemsById[bm.id]);
        }
    }

    m_treeView->expandAll();
}

void BookmarkWidget::onItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item)
        return;
    const QString path = item->data(0, Qt::UserRole + 1).toString();
    if (!path.isEmpty())
        emit directorySelected(path);
}

void BookmarkWidget::onBookmarkCurrentDirectory()
{
    if (!m_settings)
        return;

    QString currentDir = m_settings->lastUsedWorkingDirectory();
    if (currentDir.isEmpty())
        currentDir = m_settings->workingDirectory();
    if (currentDir.isEmpty()) {
        QMessageBox::information(this, tr("No directory"),
            tr("There is no current working directory to bookmark."));
        return;
    }

    Settings::BookmarkItem bm;
    bm.id = QUuid::createUuid().toString();
    bm.name = QDir(currentDir).dirName();
    bm.path = currentDir;
    bm.isFolder = false;
    bm.parentId = "";
    bm.created = QDateTime::currentDateTime();
    m_settings->addBookmark(bm);
    buildTree();
    emit bookmarksChanged();
}

void BookmarkWidget::onContextMenu(const QPoint& pos)
{
    if (!m_settings)
        return;

    QTreeWidgetItem* item = m_treeView ? m_treeView->itemAt(pos) : nullptr;
    QMenu contextMenu(this);

    if (!item) {
        QAction* addAction = contextMenu.addAction(tr("Add Current Directory as Bookmark"));
        connect(addAction, &QAction::triggered, this, &BookmarkWidget::onBookmarkCurrentDirectory);

        QAction* folderAction = contextMenu.addAction(tr("New Folder..."));
        connect(folderAction, &QAction::triggered, this, [this]() {
            QString name = QInputDialog::getText(this, tr("New Bookmark Folder"),
                                                 tr("Folder name:"));
            if (!name.isEmpty() && m_settings) {
                Settings::BookmarkItem folder;
                folder.id = QUuid::createUuid().toString();
                folder.name = name;
                folder.path = "";
                folder.isFolder = true;
                folder.parentId = "";
                folder.created = QDateTime::currentDateTime();
                m_settings->addBookmark(folder);
                buildTree();
                emit bookmarksChanged();
            }
        });
    } else {
        QString itemId = item->data(0, Qt::UserRole).toString();
        Settings::BookmarkItem bm;
        for (const auto& b : m_settings->bookmarks()) {
            if (b.id == itemId) {
                bm = b;
                break;
            }
        }

        if (bm.isFolder) {
            QAction* renameAction = contextMenu.addAction(tr("Rename..."));
            connect(renameAction, &QAction::triggered, this, [this, itemId, item]() {
                QString newName = QInputDialog::getText(this, tr("Rename Folder"),
                    tr("New name:"), QLineEdit::Normal, item->text(0));
                if (!newName.isEmpty() && m_settings) {
                    auto bookmarks = m_settings->bookmarks();
                    for (auto& b : bookmarks) {
                        if (b.id == itemId) {
                            b.name = newName;
                            m_settings->updateBookmark(itemId, b);
                            break;
                        }
                    }
                    buildTree();
                    emit bookmarksChanged();
                }
            });

            QAction* deleteAction = contextMenu.addAction(tr("Delete Folder"));
            connect(deleteAction, &QAction::triggered, this, [this, itemId]() {
                if (m_settings) {
                    m_settings->removeBookmark(itemId);
                    buildTree();
                    emit bookmarksChanged();
                }
            });
        } else {
            QAction* openAction = contextMenu.addAction(tr("Open"));
            connect(openAction, &QAction::triggered, this, [this, bm]() {
                if (!bm.path.isEmpty())
                    emit directorySelected(bm.path);
            });

            contextMenu.addSeparator();

            QAction* renameAction = contextMenu.addAction(tr("Rename..."));
            connect(renameAction, &QAction::triggered, this, [this, itemId, item]() {
                QString newName = QInputDialog::getText(this, tr("Rename Bookmark"),
                    tr("New name:"), QLineEdit::Normal, item->text(0));
                if (!newName.isEmpty() && m_settings) {
                    auto bookmarks = m_settings->bookmarks();
                    for (auto& b : bookmarks) {
                        if (b.id == itemId) {
                            b.name = newName;
                            m_settings->updateBookmark(itemId, b);
                            break;
                        }
                    }
                    buildTree();
                    emit bookmarksChanged();
                }
            });

            QAction* tagsAction = contextMenu.addAction(tr("Edit Tags..."));
            connect(tagsAction, &QAction::triggered, this, [this, itemId]() {
                if (!m_settings)
                    return;
                auto bookmarks = m_settings->bookmarks();
                for (auto& b : bookmarks) {
                    if (b.id == itemId) {
                        QString tags = QInputDialog::getText(this, tr("Edit Tags"),
                            tr("Tags (comma-separated):"), QLineEdit::Normal,
                            b.tags.join(QStringLiteral(", ")));
                        b.tags = tags.split(QStringLiteral(","), Qt::SkipEmptyParts);
                        for (QString& t : b.tags)
                            t = t.trimmed();
                        m_settings->updateBookmark(itemId, b);
                        break;
                    }
                }
                buildTree();
                emit bookmarksChanged();
            });

            QAction* deleteAction = contextMenu.addAction(tr("Delete Bookmark"));
            connect(deleteAction, &QAction::triggered, this, [this, itemId]() {
                if (m_settings) {
                    m_settings->removeBookmark(itemId);
                    buildTree();
                    emit bookmarksChanged();
                }
            });
        }
    }

    contextMenu.exec(m_treeView && m_treeView->viewport()
                     ? m_treeView->viewport()->mapToGlobal(pos)
                     : QPoint());
}
