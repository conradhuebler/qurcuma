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
    /// Expand/collapse programmatically.
    void setExpanded(bool expanded);

private:
    QToolButton* m_header = nullptr;
    QWidget* m_content = nullptr;
};
