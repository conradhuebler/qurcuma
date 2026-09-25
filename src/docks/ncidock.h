// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// NciDock — right-side dock and the one home of the non-covalent interactions: a
// collapsible "Options" section (NciOptionsWidget, UX stage 4) above the contact table
// (NciWidget). The table's signals are re-emitted so MainWindow wires to the dock and
// does not need to reach through to the inner widget.
//
// Claude Generated 2026 - NCI analysis.

#pragma once

#include "dockconfig.h"

#include <QDockWidget>
#include <QVector>

#include "../ncitypes.h"
#include "../view.h"

class NciWidget;
class CollapsibleSection;

class NciDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit NciDock(QWidget* parent = nullptr);

    NciWidget* widget() const { return m_nci; }

    void setResult(const nci::Result& result, const QVector<MoleculeViewer::Atom>& atoms);
    void setKindPalette(const nci::Palette& palette);
    void setSource(int source);
    void setBusy(bool busy);
    void setStatus(const QString& text);
    /// Claude Generated 2026 - Place the NCI options (NciOptionsWidget) above the table.
    void setOptionsWidget(QWidget* options);
    /// Open the options section (Display ▸ NCI Options…).
    void expandOptions();

signals:
    void sourceChanged(int source);
    void analysisRequested(int source);
    void contactSelected(const QVector<int>& atomIndices);
    void contactFocused(const QVector<int>& atomIndices);

private:
    NciWidget* m_nci = nullptr;
    CollapsibleSection* m_optionsSection = nullptr;
};
