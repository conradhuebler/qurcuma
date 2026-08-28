// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// NciDock — right-side dock holding the non-covalent interaction contact table.
// Thin wrapper around NciWidget; the signals are re-emitted so MainWindow wires
// to the dock and does not need to reach through to the inner widget.
//
// Claude Generated 2026 - NCI analysis.

#pragma once

#include "dockconfig.h"

#include <QDockWidget>
#include <QVector>

#include "../ncitypes.h"
#include "../view.h"

class NciWidget;

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

signals:
    void sourceChanged(int source);
    void analysisRequested(int source);
    void contactSelected(const QVector<int>& atomIndices);
    void contactFocused(const QVector<int>& atomIndices);

private:
    NciWidget* m_nci = nullptr;
};
