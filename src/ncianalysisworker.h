// ncianalysisworker.h - Off-thread NCI analysis from curcuma calculations
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026
//
// Runs a GFN-FF parameter generation (or a GFN2 single point) on its own thread
// and turns the result into an nci::Result plus the atomic charges.
//
// It deliberately does NOT share the simulation thread: that one is occupied by
// the MD timer, so an analysis posted there would stall a running simulation.
// The analysis must be usable while an MD runs, which needs a second thread by
// definition.

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "ncitypes.h"
#include "view.h"

class NciAnalysisWorker : public QObject
{
    Q_OBJECT

public:
    /// One analysis job. The atom list is copied, so the GUI thread may keep
    /// editing the structure while the calculation runs.
    struct Request {
        QVector<MoleculeViewer::Atom> atoms;
        QVector<MoleculeViewer::Bond> bonds;
        QString method = QStringLiteral("gfnff");  // gfnff | gfn2 | gfn1
        int frame = 0;
        quint64 requestId = 0;
        nci::Options options;  ///< thresholds and which interaction kinds to report
    };

    explicit NciAnalysisWorker(QObject* parent = nullptr);

public slots:
    /// Run one job. Invoked queued from the GUI thread.
    void analyse(NciAnalysisWorker::Request request);

signals:
    void resultReady(quint64 requestId, const nci::Result& result);
    /// Atomic charges of the analysed frame: EEQ for GFN-FF, Mulliken for GFN2.
    void chargesReady(quint64 requestId, int frame, const QVector<float>& charges);
    void errorOccurred(const QString& message);
};

Q_DECLARE_METATYPE(NciAnalysisWorker::Request)
