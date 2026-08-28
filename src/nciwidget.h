// nciwidget.h - Contact table of the non-covalent interaction analysis
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026
//
// The 3D overlay answers "where", this table answers "which and how strong".
// The widget holds no viewer pointer: it emits what the user asked for and
// MainWindow drives the viewer, the same split RMSDWidget uses.

#pragma once

#include <QVector>
#include <QWidget>

#include "ncitypes.h"
#include "view.h"

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

class NciWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NciWidget(QWidget* parent = nullptr);

    /// Show a new analysis result. @p atoms is the frame the contacts refer to and
    /// is used for the atom labels in the table.
    void setResult(const nci::Result& result, const QVector<MoleculeViewer::Atom>& atoms);
    /// Adopt the user's interaction colours so the table matches the 3D overlay.
    void setKindPalette(const nci::Palette& palette);
    /// Keep the source selector in sync with the Display panel.
    void setSource(int source);
    /// Disable the analyse button while a calculation is in flight.
    void setBusy(bool busy);
    /// Replace the header line, e.g. with an error from the analysis worker.
    void setStatus(const QString& text);

signals:
    /// The user picked a different overlay source (0=off, 1=geometry,
    /// 2=GFN-FF parameters, 3=population analysis).
    void sourceChanged(int source);
    /// Run the calculated source for the frame currently displayed.
    void analysisRequested(int source);
    /// A table row was activated: select these atoms in the 3D view.
    void contactSelected(const QVector<int>& atomIndices);
    /// A row was double-clicked: select and zoom to these atoms.
    void contactFocused(const QVector<int>& atomIndices);

private:
    void setupUI();
    void copyTable();
    QVector<int> atomsOfRow(int row) const;

    QComboBox* m_sourceCombo = nullptr;
    QPushButton* m_analyseButton = nullptr;
    QPushButton* m_copyButton = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QTableWidget* m_table = nullptr;

    nci::Result m_result;
    nci::Palette m_palette;
};
