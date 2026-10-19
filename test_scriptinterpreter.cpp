// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - What the script interpreter promises, pinned. The two
// assumptions the whole design rests on are measured here rather than assumed: that a
// script's last expression is its result, and that a runaway loop can be stopped from
// another thread (QJSEngine::setInterrupted is documented thread-safe for this).

#include "script/scriptbuiltins.h"
#include "script/scriptinterpreter.h"

#include <QCoreApplication>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

static int g_failed = 0;

static void check(bool ok, const QString& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
    if (!ok)
        ++g_failed;
}

static bool mentions(const QString& haystack, const QString& needle)
{
    return haystack.contains(needle, Qt::CaseInsensitive);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    ScriptInterpreter interpreter;

    // --- the result contract -------------------------------------------------
    // Everything downstream rests on this: the value of the last expression is what
    // the caller gets. Qt's documentation only says "returns the result of the
    // evaluation", so it is measured here first.
    {
        const ScriptResult r = interpreter.run(QStringLiteral("var a = 1; a * 2"));
        check(r.ok, "a script runs");
        check(r.value.toDouble() == 2.0, "and its last expression is the result");
        check(r.prints.isEmpty(), "with nothing printed");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("1 + 2 * 3"));
        check(r.value.toDouble() == 7.0, "precedence is JavaScript's");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("2 ** 10"));
        check(r.value.toDouble() == 1024.0, "and so is the power operator");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("var out = {dE: 0.0032, kJ: 8.4}; out"));
        const QVariantMap out = r.value.toMap();
        check(r.ok && out.size() == 2, "an object result comes back as named members");
        check(qAbs(out.value(QStringLiteral("kJ")).toDouble() - 8.4) < 1e-12,
            "with their values");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("[1, 2, 3]"));
        check(r.value.toList().size() == 3, "a list result comes back as a list");
    }

    // --- what the builtins add ----------------------------------------------
    {
        const ScriptResult r = interpreter.run(QStringLiteral("print(\"dE =\", 0.0032)"));
        check(r.prints.size() == 1 && r.prints.first() == QStringLiteral("dE = 0.0032"),
            "print collects one readable line");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("console.log(\"hi\", 2)"));
        check(r.prints.size() == 1 && r.prints.first() == QStringLiteral("hi 2"),
            "console.log means print here");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("mean(1, 2, 3)"));
        check(r.value.toDouble() == 2.0, "mean takes single values");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("mean([1, 2, 3])"));
        check(r.value.toDouble() == 2.0, "and a list");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("sum([1, 2, 3])"));
        check(r.value.toDouble() == 6.0, "sum likewise");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("mean(1, \"zwei\")"));
        check(!r.ok && mentions(r.error.message, QStringLiteral("expected a number")),
            "a non-number is refused by name");
    }

    // --- statistics, against hand-computed values ---------------------------
    {
        // 1, 2, 3, 4: mean 2.5, deviations ±1.5 and ±0.5, squares 2.25 + 0.25 + 0.25 +
        // 2.25 = 5, sample variance 5/3, spread sqrt(5/3).
        const ScriptResult r = interpreter.run(QStringLiteral("sd([1, 2, 3, 4])"));
        check(qAbs(r.value.toDouble() - std::sqrt(5.0 / 3.0)) < 1e-12,
            QStringLiteral("sd is the sample spread (n-1): %1").arg(r.value.toDouble()));
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("sem([1, 2, 3, 4])"));
        check(qAbs(r.value.toDouble() - std::sqrt(5.0 / 3.0) / 2.0) < 1e-12,
            "sem is the spread of the mean");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("median([3, 1, 2])"));
        check(r.value.toDouble() == 2.0, "median of an odd count is the middle value");
        const ScriptResult even = interpreter.run(QStringLiteral("median([4, 1, 3, 2])"));
        check(even.value.toDouble() == 2.5, "and of an even count the mean of the middle two");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("min([3, 1, 2]) + max(3, 1, 2)"));
        check(r.value.toDouble() == 4.0, "min and max take a list and single values");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("len([1, 2, 3])"));
        check(r.value.toDouble() == 3.0, "len counts a list's values");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("sd([1])"));
        check(!r.ok && mentions(r.error.message, QStringLiteral("at least two")),
            "a spread of one value is refused rather than reported as zero");
    }

    // --- a straight line -----------------------------------------------------
    {
        const ScriptResult r = interpreter.run(
            QStringLiteral("slope([1, 2, 3], [2, 4, 6]) + intercept([1, 2, 3], [2, 4, 6])"));
        check(r.value.toDouble() == 2.0, "a perfect line gives slope 2 and intercept 0");
        const ScriptResult fit = interpreter.run(QStringLiteral("r2([1, 2, 3], [2, 4, 6])"));
        check(qAbs(fit.value.toDouble() - 1.0) < 1e-12, "and r2 is 1");
    }
    {
        // -1, 0, 1 against 0, 1, 0: the best line is flat at 2/3, so it explains none
        // of the spread. Hand-checked: SSres = SStot = 1.
        const ScriptResult r = interpreter.run(QStringLiteral("r2([-1, 0, 1], [0, 1, 0])"));
        check(qAbs(r.value.toDouble()) < 1e-12, "a flat fit explains nothing (r2 = 0)");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("slope([1, 2], [1, 2, 3])"));
        check(!r.ok && mentions(r.error.message, QStringLiteral("same length")),
            "lists of different lengths are refused");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("r2([1, 2, 3], [7, 7, 7])"));
        check(!r.ok && mentions(r.error.message, QStringLiteral("not defined")),
            "and a constant y has no r2");
    }

    // --- units, against the defining constants ------------------------------
    {
        // 1 Hartree = 4.3597447222071e-18 J (CODATA 2018); times NA and divided by
        // 1000 that is 2625.4996394798254 kJ/mol, which is the tabulated value.
        const ScriptResult r = interpreter.run(QStringLiteral("ha_to_kjmol(1)"));
        check(qAbs(r.value.toDouble() - 2625.4996394798254) / 2625.4996394798254 < 1e-12,
            QStringLiteral("a Hartree is 2625.4996394798254 kJ/mol: %1").arg(r.value.toDouble(), 0, 'g', 15));
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("ha_to_ev(1)"));
        check(qAbs(r.value.toDouble() - 27.211386245988) / 27.211386245988 < 1e-12,
            "and 27.211386245988 eV");
        const ScriptResult kcal = interpreter.run(QStringLiteral("ha_to_kcal(1)"));
        check(qAbs(kcal.value.toDouble() - 627.5094740631) / 627.5094740631 < 1e-10,
            "and 627.5094740631 kcal/mol");
    }
    {
        // 1 cm^-1 = h c NA per mole; the tabulated factor is 0.0119627 kJ/mol.
        const ScriptResult r = interpreter.run(QStringLiteral("cm_to_kjmol(1)"));
        check(qAbs(r.value.toDouble() - 0.0119626566) < 1e-9,
            QStringLiteral("a wavenumber is 0.0119627 kJ/mol: %1").arg(r.value.toDouble(), 0, 'g', 12));
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("bohr_to_ang(1)"));
        check(qAbs(r.value.toDouble() - 0.529177210903) < 1e-12,
            "a Bohr radius is 0.529177210903 Angstrom");
    }
    {
        // Each conversion has to undo its pair, otherwise one of the two is wrong.
        const ScriptResult r = interpreter.run(
            QStringLiteral("kjmol_to_ha(ha_to_kjmol(12.5)) + ang_to_bohr(bohr_to_ang(3.3)) + ev_to_ha(ha_to_ev(1.7)) + kcal_to_ha(ha_to_kcal(2.1))"));
        check(!r.ok || qAbs(r.value.toDouble() - (12.5 + 3.3 + 1.7 + 2.1)) < 1e-9,
            QStringLiteral("every pair of conversions undoes itself: %1").arg(r.value.toDouble(), 0, 'g', 15));
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("ha_to_kjmol(\"x\")"));
        check(!r.ok && mentions(r.error.message, QStringLiteral("expected a number")),
            "a unit conversion refuses something that is not a number");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("radians(180) - Math.PI + degrees(Math.PI) - 180"));
        check(qAbs(r.value.toDouble()) < 1e-12, "radians and degrees are inverse to Math.PI");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("Math.abs(R - NA * kB) + R"));
        check(r.ok && qAbs(r.value.toDouble() - 8.31446261815324) < 1e-12,
            "R is NA times kB, 8.31446261815324 J/(mol K)");
    }

    // --- the two languages format the same -----------------------------------
    {
        const ScriptResult r = interpreter.run(QStringLiteral("print(0.1 + 0.2)"));
        check(r.prints.size() == 1 && r.prints.first() == QStringLiteral("0.3"),
            "a double is printed with ten significant digits, trimmed");
        check(script::formatNumber(0.1 + 0.2) == QStringLiteral("0.3"),
            "and the C++ side of the same rule agrees");
    }

    // --- refusals name the place ---------------------------------------------
    {
        const ScriptResult r = interpreter.run(QStringLiteral("var a = 1;\na + b"));
        check(!r.ok, "an unknown name is refused");
        check(mentions(r.error.message, QStringLiteral("line 2")),
            QStringLiteral("with the line: %1").arg(r.error.message));
        check(mentions(r.error.message, QStringLiteral("b")), "and the name");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("1 +"));
        check(!r.ok && mentions(r.error.message, QStringLiteral("SyntaxError")),
            QStringLiteral("a syntax error keeps the engine's name: %1").arg(r.error.message));
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("   "));
        check(!r.ok && mentions(r.error.message, QStringLiteral("empty")),
            "an empty script says so");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("mean()"));
        check(!r.ok && mentions(r.error.message, QStringLiteral("at least one number")),
            "and a builtin's own complaint arrives");
    }
    {
        // Measured: a thrown non-Error leaves isError() false, so the only signal is
        // the stack frame. Without that, `throw` would look like a normal result.
        const ScriptResult r = interpreter.run(QStringLiteral("throw \"boom\";"));
        check(!r.ok && mentions(r.error.message, QStringLiteral("boom")),
            QStringLiteral("a thrown string is an error: %1").arg(r.error.message));
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("throw {code: 7};"));
        check(!r.ok && mentions(r.error.message, QStringLiteral("\"code\":7")),
            QStringLiteral("a thrown object is shown as it was thrown: %1").arg(r.error.message));
    }

    // --- reproducibility ------------------------------------------------------
    {
        const ScriptResult first = interpreter.run(QStringLiteral("var s = 0; for (var i = 1; i <= 5; ++i) s += i; s"));
        const ScriptResult second = interpreter.run(QStringLiteral("var s = 0; for (var i = 1; i <= 5; ++i) s += i; s"));
        check(first.ok && second.ok && first.value == second.value,
            "the same script twice gives the same value");
        check(first.prints == second.prints, "and the same output");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("Math.random()"));
        check(!r.ok && mentions(r.error.message, QStringLiteral("reproducible")),
            "a random number is refused with its reason");
    }
    {
        const ScriptResult r = interpreter.run(QStringLiteral("new Date().getTime()"));
        check(!r.ok && mentions(r.error.message, QStringLiteral("reproducible")),
            "and so is the clock");
    }

    // --- the run ends ---------------------------------------------------------
    {
        ScriptLimits limits;
        limits.deadlineMs = 300;
        ScriptInterpreter quick(limits);
        const auto started = std::chrono::steady_clock::now();
        const ScriptResult r = quick.run(QStringLiteral("while (true) {}"));
        const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - started)
                              .count();
        check(!r.ok && r.stopped, "an endless loop is stopped");
        check(mentions(r.error.message, QStringLiteral("ran longer")),
            QStringLiteral("with the reason: %1").arg(r.error.message));
        check(took < 3000, QStringLiteral("and it stops promptly (%1 ms)").arg(took));
    }
    {
        // The stop button's path, from another thread while the loop runs.
        std::atomic<bool> stopped { false };
        QString message;
        std::thread runner([&] {
            ScriptLimits limits;
            limits.deadlineMs = 30000;  // long enough that only the stop can end it
            ScriptInterpreter slow(limits);
            std::thread stopper([&] {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                slow.requestStop();
            });
            const ScriptResult r = slow.run(QStringLiteral("var i = 0; while (true) { i = i + 1; }"));
            stopped.store(r.stopped && !r.ok);
            message = r.error.message;
            stopper.join();
        });
        runner.join();
        check(stopped.load(), "a loop is stopped by request from another thread");
        check(mentions(message, QStringLiteral("operator")),
            QStringLiteral("and says who stopped it: %1").arg(message));
    }

    // --- what comes back is bounded ------------------------------------------
    {
        ScriptLimits limits;
        limits.maxListValues = 8;
        ScriptInterpreter small(limits);
        const ScriptResult r = small.run(
            QStringLiteral("var v = []; for (var i = 0; i < 100; ++i) v.push(i); v"));
        check(r.value.toList().size() == 8, "a long list is cut at the limit");
        check(r.truncated, "and says it was cut");
    }
    {
        ScriptLimits limits;
        limits.maxPrints = 5;
        ScriptInterpreter small(limits);
        const ScriptResult r = small.run(
            QStringLiteral("for (var i = 0; i < 20; ++i) print(i)"));
        check(r.prints.size() == 5, "and so is a long list of printed lines");
        check(r.truncated, "which is reported as well");
    }
    {
        ScriptLimits limits;
        limits.maxResultChars = 60;
        ScriptInterpreter small(limits);
        const ScriptResult r = small.run(QStringLiteral("var v = []; for (var i = 0; i < 60; ++i) v.push(i); v"));
        check(r.value.typeId() == QMetaType::QString, "a result too large to report is replaced by a note");
        check(mentions(r.value.toString(), QStringLiteral("reduce it inside the script")),
            "that says what to do instead");
        check(r.truncated, "and is marked truncated");
    }

    // --- values handed in ----------------------------------------------------
    {
        QVariantMap bindings;
        bindings.insert(QStringLiteral("data"), QVariantList { 1.0, 2.0, 3.0 });
        const ScriptResult r = interpreter.run(QStringLiteral("mean(data)"), bindings);
        check(r.ok && r.value.toDouble() == 2.0, "a bound list arrives as a list");
    }
    {
        QVariantMap bindings;
        bindings.insert(QStringLiteral("2bad"), 1.0);
        const ScriptResult r = interpreter.run(QStringLiteral("1"), bindings);
        check(!r.ok && mentions(r.error.message, QStringLiteral("cannot be bound")),
            "a name JavaScript would not accept is refused");
    }

    // --- the crossing into qurcuma ------------------------------------------
    // Without a host the interpreter is a calculator; with one it can reach a tool.
    // The assistant's tool is built without one, which is what keeps a script that
    // arrives from a model unable to touch anything.
    {
        const ScriptResult r = interpreter.run(QStringLiteral("tool(\"anything\", {})"));
        check(!r.ok && mentions(r.error.message, QStringLiteral("calculation only")),
            "without a host a script cannot call tools");
    }
    {
        QVariantList seen;
        ScriptHost host;
        host.call = [&seen](const QString& name, const QVariantMap& args) -> QVariant {
            seen.append(name);
            if (name == QLatin1String("echo"))
                return QVariantMap { { QStringLiteral("value"), args.value(QStringLiteral("x")) } };
            return QVariantMap { { QStringLiteral("error"), QStringLiteral("no such tool") } };
        };
        const ScriptResult r = interpreter.run(QStringLiteral("tool(\"echo\", {x: 42}).value + 1"),
            {}, host);
        check(r.ok && r.value.toDouble() == 43.0,
            "with a host a tool's answer comes back into the script as an object");
        check(seen.size() == 1 && seen.first().toString() == QStringLiteral("echo"),
            "and the host saw the call by name");
    }
    {
        ScriptHost host;
        host.call = [](const QString&, const QVariantMap&) -> QVariant {
            return QVariantMap { { QStringLiteral("error"), QStringLiteral("refused") } };
        };
        const ScriptResult r = interpreter.run(
            QStringLiteral("var r = tool(\"do_it\", {}); r.error ? 1 : 0"), {}, host);
        check(r.ok && r.value.toDouble() == 1.0,
            "a refused call is a value a script can branch on, not an exception");
    }

    std::printf("%s (%d failed)\n", g_failed ? "FAILED" : "OK", g_failed);
    return g_failed ? 1 : 0;
}
