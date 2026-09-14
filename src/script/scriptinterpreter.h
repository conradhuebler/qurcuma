// scriptinterpreter.h - Runs a short JavaScript calculation, with limits.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - SupraFit has a scripting engine so that an equation is
// computed by the application instead of by whoever wrote it down. qurcuma has the
// tool layer, which reaches curcuma for energies and trajectories, but nothing for
// arithmetic on numbers that are already in hand: a difference, a ratio, a unit
// change, a mean, a spread, a slope. This is that half.
//
// The engine is QJSEngine, which arrives with Qt6Qml and is already in the build for
// the 3D viewer. JavaScript is chosen over a purpose-built grammar because a model
// writes it reliably and its arithmetic (`Math.log` against `Math.log10`, arrays,
// objects) needs no teaching.
//
// Every run gets a fresh engine, so nothing survives from one script to the next and
// the same source with the same bindings yields the same result. Date and
// Math.random are removed for the same reason.
//
// Headless and free of Qt Widgets, the viewer and curcuma: a script is a calculation,
// not a way to reach the application. The host bridge (see ScriptHost) is the one
// place where that is deliberately opened, and it is opened by the caller, not here.
#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <functional>

/// What a run may spend. The defaults are the contract the tool description states,
/// so a model can plan a loop that ends.
struct ScriptLimits {
    int deadlineMs = 5000;      ///< wall clock for the whole run; 0 or less disables it.
                                ///< The stop button does not depend on it and always works.
    int maxResultChars = 2000;  ///< the reported value as compact JSON
    int maxListValues = 64;     ///< values kept per reported list or object
    int maxPrints = 32;         ///< lines kept from print()
};

/// Where a script went wrong. The engine reports line numbers and stack frames, so a
/// refusal can name the place instead of describing the whole script back.
struct ScriptError {
    QString message;  ///< what is wrong, in the words a reader of the tool result needs
    int line = 0;     ///< 1-based; 0 when the engine gave no position (a builtin's throw)
    int column = 0;

    bool isEmpty() const { return message.isEmpty(); }
};

/// What a run produced. `value` is the value of the script's last expression: a
/// number, a list, or an object whose members are the named results.
struct ScriptResult {
    bool ok = false;
    QVariant value;
    QStringList prints;    ///< the labels print() collected, in order
    qint64 elapsedMs = 0;
    bool truncated = false;  ///< a list, an object or the value as a whole was cut
    bool stopped = false;    ///< the deadline or the operator ended the run early
    ScriptError error;
};

/// What a script may reach beyond arithmetic. An interpreter built without a host is
/// a calculator; that is how the tool the assistant calls is built, so a script that
/// arrives from a model cannot touch anything but its own numbers.
///
/// Claude Generated 2026 - Qt 6.11's QJSEngine has no newFunction(), so the crossing
/// goes through a QObject (ScriptBridge, scriptbridge.h) whose one Q_INVOKABLE method
/// is this callback. That is measured, not assumed: neither the installed header nor
/// the installed documentation lists a way to create a JS function from C++.
struct ScriptHost {
    /// Called for `tool(name, args)`. Returns the tool's result, usually a map of
    /// the kind a tool hands back. A failure is reported by returning a map that
    /// carries "error" instead of throwing.
    std::function<QVariant(const QString& name, const QVariantMap& args)> call;
};

/// One script, one engine, one result. Not copyable: it carries the stop flag that a
/// Stop button in another thread sets.
class ScriptInterpreter {
public:
    explicit ScriptInterpreter(ScriptLimits limits = {});
    ~ScriptInterpreter();

    ScriptInterpreter(const ScriptInterpreter&) = delete;
    ScriptInterpreter& operator=(const ScriptInterpreter&) = delete;

    /// Run @p source. @p bindings become globals before the script starts (`data`
    /// for the values a caller passes in). Blocking; call it from a worker thread
    /// when the caller has an event loop to keep turning.
    ///
    /// A stop requested before the call does not carry over: a run always starts
    /// un-stopped, which is why there is no clearStop() to forget.
    ScriptResult run(const QString& source, const QVariantMap& bindings = {},
        const ScriptHost& host = {});

    /// Ask a run in progress to give up. Safe from any thread; that is what makes it
    /// usable as a Stop button.
    void requestStop() { m_stop.store(true); }
    bool isStopRequested() const { return m_stop.load(); }

    /// A caller's own reason to stop, asked while the script runs: the tool dispatcher
    /// has an interrupt flag of its own, and this is how it reaches the engine. The
    /// poll is called from the watching thread, so it has to be thread-safe (the
    /// dispatcher's flag is).
    void setStopPoll(std::function<bool()> poll) { m_stopPoll = std::move(poll); }

private:
    ScriptLimits m_limits;
    std::atomic<bool> m_stop { false };
    std::function<bool()> m_stopPoll;
};
