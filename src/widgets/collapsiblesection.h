// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// CollapsibleSection — a titled header button that shows/hides a content widget,
// optionally with a switch in the header for a feature whose settings are the
// content. Used by the Appearance, Interactions and Simulation docks. Claude Generated.
#pragma once

#include <QWidget>

class QCheckBox;
class QHBoxLayout;
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
    /// The header text. Claude Generated 2026 - the chat transcript labels each
    /// folded section by it, so it has to be readable back out.
    QString title() const;

    /// Claude Generated 2026 - Put a switch in front of the title: the "enable" of a
    /// feature whose settings are the content. Switching on expands the section and
    /// enables the content; switching off collapses it and disables the content,
    /// which can still be expanded to look at. Returns the switch (for tooltips and
    /// for locking it while a run is active). Starts off.
    QCheckBox* addSwitch(const QString& toolTip = QString());
    /// Set the switch and the matching expand/enable state without emitting
    /// switchToggled (e.g. when a stored configuration is applied).
    void setSwitchedOn(bool on);
    bool isSwitchedOn() const;

signals:
    /// Claude Generated 2026 - fired on every expand/collapse (user click or
    /// setExpanded), so the host can persist the state.
    void expandedChanged(bool expanded);
    /// The user flipped the header switch (not emitted by setSwitchedOn).
    void switchToggled(bool on);

private:
    void applySwitch(bool on);

    QHBoxLayout* m_headerRow = nullptr;
    QToolButton* m_header = nullptr;
    QCheckBox* m_switch = nullptr;
    QWidget* m_content = nullptr;
};
