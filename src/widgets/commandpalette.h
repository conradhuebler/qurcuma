// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// CommandPalette — a searchable Ctrl+K popup that lists and runs the app's
// commands (collected from the menus + a few curated viewer commands).
// Claude Generated 2026.
#pragma once

#include <QVector>
#include <QWidget>
#include <functional>

class QLineEdit;
class QListWidget;

class CommandPalette : public QWidget
{
    Q_OBJECT
public:
    struct Command {
        QString title;     // e.g. "Open File…"
        QString context;   // menu path, e.g. "File"
        QString shortcut;  // e.g. "Ctrl+O"
        std::function<void()> run;
        bool enabled = true;
    };

    explicit CommandPalette(QWidget* parent = nullptr);

    void setCommands(const QVector<Command>& cmds);
    /// Show centered over the parent window, cleared and focused.
    void popUp();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refilter();
    void executeRow(int row);

    QLineEdit* m_search = nullptr;
    QListWidget* m_list = nullptr;
    QVector<Command> m_commands;
    QVector<int> m_filtered; // indices into m_commands, in display order
};
