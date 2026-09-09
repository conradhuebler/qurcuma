// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// CollapsibleSection — a titled header button that shows/hides a content widget.
// Used to build the accordion-style Display dock. Claude Generated.
#pragma once

#include <QWidget>

class QToolButton;
class QLayout;

class CollapsibleSection : public QWidget
{
    Q_OBJECT
public:
    explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr);

    /// Place a layout (with its widgets) into the collapsible content area.
    void setContentLayout(QLayout* layout);
    /// Change the header text after construction. Claude Generated 2026 - the
    /// assistant's reasoning section counts characters into its own title.
    void setTitle(const QString& title);
    /// Expand/collapse programmatically.
    void setExpanded(bool expanded);
    /// Current expand state (for persistence). Claude Generated 2026.
    bool isExpanded() const;

signals:
    /// Claude Generated 2026 - fired on every expand/collapse (user click or
    /// setExpanded), so the host can persist the state.
    void expandedChanged(bool expanded);

private:
    QToolButton* m_header = nullptr;
    QWidget* m_content = nullptr;
};
