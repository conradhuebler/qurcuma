// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// ElementQuickBar — compact element strip for the molecule builder: the common
// organic-chemistry elements as CPK-tinted toggle buttons plus a "…" button
// opening a full periodic-table popup. Claude Generated 2026.
#pragma once

#include <QHash>
#include <QWidget>

class QToolButton;
class QHBoxLayout;

/** @brief Full 18-column periodic table (H..Og) as a popup grid; clicking an
 *  element emits elementPicked and closes the popup. Positions follow the
 *  standard layout with lanthanides/actinides in two separate rows. */
class PeriodicTableWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PeriodicTableWidget(QWidget* parent = nullptr);

signals:
    void elementPicked(const QString& symbol);
};

class ElementQuickBar : public QWidget
{
    Q_OBJECT
public:
    explicit ElementQuickBar(QWidget* parent = nullptr);

    /// Highlight @p symbol if it is one of the quick buttons (hotkey/picker sync).
    void setCurrentElement(const QString& symbol);

signals:
    void elementPicked(const QString& symbol);

private:
    QHash<QString, QToolButton*> m_buttons;
    PeriodicTableWidget* m_table = nullptr;
};
