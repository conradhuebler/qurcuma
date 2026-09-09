// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// OutputDock — bottom dock holding the read-only calculation/output log.
// Replaces the inline output QTextEdit previously created in MainWindow.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include "dockconfig.h"

#include <QDockWidget>
#include <QStringList>

class QTextEdit;

class LogHub;      // Claude Generated 2026 - src/core/loghub.h
class QTimer;
struct LogRecord;  // Claude Generated 2026 - src/core/loghub.h

class OutputDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit OutputDock(QWidget* parent = nullptr);

    /// Raw access to the log view for callers that already append text directly.
    QTextEdit* outputView() const;

    /// Mirror @p hub into the view: first what it already holds, then live records.
    /// Appends are coalesced on a short timer -- qDebug() reaches the hub once per
    /// optimizer iteration during a grab (simulationworker.cpp), and one
    /// QTextEdit::append per line stalls the GUI at that rate.
    /// Claude Generated 2026.
    void followLogHub(LogHub* hub);

public slots:
    void appendOutput(const QString& text);
    void clearOutput();
    /// Copy the log to the clipboard: the selection if there is one, otherwise the
    /// whole thing. Claude Generated 2026 - the window-wide Ctrl+C used to reach
    /// the structure text no matter where the focus was, so a log line could not
    /// be got out of the program at all.
    void copyOutput();
    /// Replace the whole log with @p text; optionally scroll to the bottom.
    void setText(const QString& text, bool scrollToBottom = false);

signals:
    /// Emitted when the user clicks the clear button.
    void clearRequested();

private:
    void setupUI();
    void flushPending();

    QTextEdit* m_outputView = nullptr;
    QStringList m_pending;             ///< records waiting for the next flush
    QTimer* m_flushTimer = nullptr;    ///< created on the first followLogHub()
};
