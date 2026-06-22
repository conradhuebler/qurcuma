// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// CommandPalette. Claude Generated 2026.
#include "commandpalette.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <algorithm>

CommandPalette::CommandPalette(QWidget* parent)
    : QWidget(parent, Qt::Popup)
{
    setObjectName(QStringLiteral("CommandPalette"));
    setFixedSize(620, 420);

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(8, 8, 8, 8);
    col->setSpacing(6);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Type a command…  (↑/↓ to navigate, Enter to run, Esc to close)"));
    m_search->setClearButtonEnabled(true);
    col->addWidget(m_search);

    m_list = new QListWidget(this);
    m_list->setUniformItemSizes(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setFocusPolicy(Qt::NoFocus); // keep typing focus in the search field
    col->addWidget(m_list, 1);

    setStyleSheet(QStringLiteral(
        "QWidget#CommandPalette { background: palette(window); border: 1px solid palette(mid); }"
        "QLineEdit { padding: 6px 8px; font-size: 14px; }"
        "QListWidget { border: none; }"
        "QListWidget::item { padding: 5px 6px; }"));

    connect(m_search, &QLineEdit::textChanged, this, [this]() { refilter(); });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        executeRow(m_list->row(it));
    });

    m_search->installEventFilter(this);
}

void CommandPalette::setCommands(const QVector<Command>& cmds)
{
    m_commands = cmds;
    refilter();
}

void CommandPalette::popUp()
{
    m_search->clear();
    refilter();
    if (QWidget* p = parentWidget() ? parentWidget()->window() : nullptr) {
        const QRect g = p->geometry();
        move(g.center().x() - width() / 2, g.top() + qMax(60, g.height() / 6));
    }
    show();
    raise();
    m_search->setFocus(Qt::ShortcutFocusReason);
}

void CommandPalette::refilter()
{
    const QString needle = m_search ? m_search->text().trimmed().toLower() : QString();
    m_filtered.clear();

    // Score: lower is better. Prefix > word-start > contains(title) > contains(context).
    QVector<QPair<int, int>> scored; // (score, command index)
    for (int i = 0; i < m_commands.size(); ++i) {
        const Command& c = m_commands[i];
        const QString title = c.title.toLower();
        const QString ctx = c.context.toLower();
        int score = -1;
        if (needle.isEmpty()) {
            score = 0;
        } else if (title.startsWith(needle)) {
            score = 0;
        } else {
            bool wordStart = false;
            for (const QString& w : title.split(' ', Qt::SkipEmptyParts))
                if (w.startsWith(needle)) { wordStart = true; break; }
            if (wordStart)
                score = 1;
            else if (title.contains(needle))
                score = 2;
            else if (ctx.contains(needle))
                score = 3;
        }
        if (score >= 0)
            scored.append({ score, i });
    }
    std::stable_sort(scored.begin(), scored.end(), [this](const QPair<int, int>& a, const QPair<int, int>& b) {
        if (a.first != b.first)
            return a.first < b.first;
        return m_commands[a.second].title.compare(m_commands[b.second].title, Qt::CaseInsensitive) < 0;
    });

    m_list->clear();
    for (const auto& s : scored) {
        const Command& c = m_commands[s.second];
        m_filtered.append(s.second);
        QString label = c.title;
        if (!c.context.isEmpty())
            label += QStringLiteral("    ·  %1").arg(c.context);
        if (!c.shortcut.isEmpty())
            label += QStringLiteral("    [%1]").arg(c.shortcut);
        auto* item = new QListWidgetItem(label, m_list);
        if (!c.enabled)
            item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

void CommandPalette::executeRow(int row)
{
    if (row < 0 || row >= m_filtered.size())
        return;
    const Command c = m_commands[m_filtered[row]];
    close();
    if (c.enabled && c.run)
        c.run(); // run after closing so any dialog it opens shows cleanly
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_search && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        switch (ke->key()) {
        case Qt::Key_Down:
            if (m_list->count() > 0)
                m_list->setCurrentRow(qMin(m_list->currentRow() + 1, m_list->count() - 1));
            return true;
        case Qt::Key_Up:
            if (m_list->count() > 0)
                m_list->setCurrentRow(qMax(m_list->currentRow() - 1, 0));
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            executeRow(m_list->currentRow());
            return true;
        case Qt::Key_Escape:
            close();
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}
