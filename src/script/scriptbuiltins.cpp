// scriptbuiltins.cpp - The builtin table, written in the language it serves.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - The table is JavaScript rather than C++ for two reasons.
// It reads as what it is (a formula a chemist can check against a textbook), and the
// engine offers no way to define a function from C++ in this Qt version, so a C++
// builtin would need a QObject per function. Only print crosses back, and that
// crossing is an array read after the run.
#include "scriptbuiltins.h"

#include <QJSEngine>
#include <QJSValue>

namespace script {

namespace {

const char* const kBuiltins = R"JS(
// --- printing --------------------------------------------------------------
// print(label, value, ...) collects one line. C++ reads the array back after the run;
// nothing here touches the outside world.
var __prints = [];

function __render(value) {
    if (value === null) return "null";
    if (value === undefined) return "undefined";
    if (typeof value === "number") return __num(value);
    if (typeof value === "string") return value;
    if (typeof value === "boolean") return value ? "true" : "false";
    if (Array.isArray(value)) return "[" + value.map(__render).join(", ") + "]";
    if (typeof value === "object") {
        var parts = [];
        for (var key in value) {
            if (Object.prototype.hasOwnProperty.call(value, key))
                parts.push(key + ": " + __render(value[key]));
        }
        return "{" + parts.join(", ") + "}";
    }
    return String(value);
}

// Ten significant digits, trailing zeros gone. Same rule as script::formatNumber.
function __num(value) {
    if (Number.isNaN(value)) return "nan";
    if (!Number.isFinite(value)) return value > 0 ? "inf" : "-inf";
    var text = value.toPrecision(10);
    if (text.indexOf("e") >= 0) return text;
    if (text.indexOf(".") >= 0) text = text.replace(/0+$/, "").replace(/\.$/, "");
    return text;
}

function print() {
    var parts = [];
    for (var i = 0; i < arguments.length; ++i) parts.push(__render(arguments[i]));
    __prints.push(parts.join(" "));
}

// A model reaches for console.log out of habit; it means print here.
var console = { log: print, info: print, warn: print, error: print };

// --- reproducibility --------------------------------------------------------
// Not a sandbox: the point is that the same script gives the same numbers. A script
// that needs random numbers defines its own generator, which is reproducible again.
function __noClock() {
    throw new Error("Date is not available here: a run must be reproducible");
}
Date = __noClock;
Math.random = __noClock;

// --- the crossing into qurcuma, when the caller installed one ---------------
// The tool the assistant calls runs without __host, so this refuses there.
function tool(name, args) {
    if (typeof __host === "undefined" || __host === null)
        throw new Error("this script is a calculation only: it cannot call tools");
    return __host.tool(String(name), (args === undefined || args === null) ? {} : args);
}

// --- numbers out of arguments ----------------------------------------------
// Both spellings work: mean([1, 2, 3]) and mean(1, 2, 3). A model writes either, and
// refusing one of them costs a round for nothing.
function __numbers(args) {
    var values = [];
    if (args.length === 1 && Array.isArray(args[0])) {
        for (var i = 0; i < args[0].length; ++i) values.push(__number(args[0][i], i));
    } else {
        for (var j = 0; j < args.length; ++j) values.push(__number(args[j], j));
    }
    return values;
}

function __number(value, at) {
    if (typeof value !== "number" || Number.isNaN(value))
        throw new Error("expected a number at position " + at + ", got " + __render(value));
    return value;
}

// One number out of one argument, for the functions that convert a value rather than
// reduce a list.
function __one(value) {
    return __number(value, 0);
}

function sum() {
    var values = __numbers(arguments);
    var total = 0;
    for (var i = 0; i < values.length; ++i) total += values[i];
    return total;
}

function mean() {
    var values = __numbers(arguments);
    if (values.length === 0) throw new Error("mean needs at least one number");
    return sum(values) / values.length;
}

function len() {
    return __numbers(arguments).length;
}

// The spread of a sample, divided by n-1: measured values are a sample of something,
// and the population formula would report a spread that is too small. The tool
// description says so, because "sd" alone does not.
function sd() {
    var values = __numbers(arguments);
    if (values.length < 2) throw new Error("sd needs at least two numbers");
    var m = mean(values);
    var squares = 0;
    for (var i = 0; i < values.length; ++i) squares += (values[i] - m) * (values[i] - m);
    return Math.sqrt(squares / (values.length - 1));
}

// The standard error of the mean: how far the mean of this sample is likely to be
// from the mean of the thing it was drawn from.
function sem() {
    var values = __numbers(arguments);
    return sd(values) / Math.sqrt(values.length);
}

function median() {
    var values = __numbers(arguments).slice().sort(function (a, b) { return a - b; });
    if (values.length === 0) throw new Error("median needs at least one number");
    var half = Math.floor(values.length / 2);
    return values.length % 2 ? values[half] : 0.5 * (values[half - 1] + values[half]);
}

// JavaScript has Math.min and Math.max but nothing that takes a list, which is the
// case a script has. The global names min/max are free.
function min() {
    var values = __numbers(arguments);
    if (values.length === 0) throw new Error("min needs at least one number");
    return Math.min.apply(null, values);
}

function max() {
    var values = __numbers(arguments);
    if (values.length === 0) throw new Error("max needs at least one number");
    return Math.max.apply(null, values);
}

// --- a straight line through measured points --------------------------------
// Least squares. Three functions rather than one that returns a record: a script that
// needs only the slope should not have to name the other two.
function __pair(xs, ys) {
    var x = __numbers([xs]);
    var y = __numbers([ys]);
    if (x.length !== y.length)
        throw new Error("the two lists must be the same length, got " + x.length + " and " + y.length);
    if (x.length < 2) throw new Error("a line needs at least two points");
    return [x, y];
}

function slope(xs, ys) {
    var pair = __pair(xs, ys), x = pair[0], y = pair[1];
    var mx = mean(x), my = mean(y), sxy = 0, sxx = 0;
    for (var i = 0; i < x.length; ++i) {
        sxy += (x[i] - mx) * (y[i] - my);
        sxx += (x[i] - mx) * (x[i] - mx);
    }
    if (sxx === 0) throw new Error("all x values are the same: a slope is not defined");
    return sxy / sxx;
}

function intercept(xs, ys) {
    var pair = __pair(xs, ys);
    return mean(pair[1]) - slope(pair[0], pair[1]) * mean(pair[0]);
}

/// The fraction of the spread in y that the line explains, between 0 and 1.
function r2(xs, ys) {
    var pair = __pair(xs, ys), x = pair[0], y = pair[1];
    var a = intercept(x, y), b = slope(x, y);
    var my = mean(y), residual = 0, total = 0;
    for (var i = 0; i < x.length; ++i) {
        residual += (y[i] - (a + b * x[i])) * (y[i] - (a + b * x[i]));
        total += (y[i] - my) * (y[i] - my);
    }
    if (total === 0) throw new Error("all y values are the same: r2 is not defined");
    return 1 - residual / total;
}

// --- units ------------------------------------------------------------------
// Constants first, conversions derived from them, so a reader can check each step
// against a textbook instead of trusting a quoted number. Sources: the SI defining
// constants (2019) for kB, NA, h, c and the elementary charge; CODATA 2018 for the
// Hartree and the Bohr radius.
var kB = 1.380649e-23;             // J/K
var NA = 6.02214076e23;            // 1/mol
var h = 6.62607015e-34;            // J s
var c = 299792458;                 // m/s
var R = NA * kB;                   // J/(mol K) = 8.31446261815324
var __elementary = 1.602176634e-19;  // C
var __bohr = 5.29177210903e-11;    // m
var __hartree = 4.3597447222071e-18;  // J

function ha_to_kjmol(x) { return __one(x) * __hartree * NA / 1000; }
function kjmol_to_ha(x) { return __one(x) * 1000 / (__hartree * NA); }
function ha_to_ev(x) { return __one(x) * __hartree / __elementary; }
function ev_to_ha(x) { return __one(x) * __elementary / __hartree; }
// 1 cal = 4.184 J, the thermochemical calorie.
function ha_to_kcal(x) { return ha_to_kjmol(x) / 4.184; }
function kcal_to_ha(x) { return kjmol_to_ha(__one(x) * 4.184); }
function bohr_to_ang(x) { return __one(x) * __bohr / 1e-10; }
function ang_to_bohr(x) { return __one(x) * 1e-10 / __bohr; }
// A wavenumber is an energy per mole: E = h c NA / lambda, and 1 cm^-1 = 100 m^-1.
function cm_to_kjmol(x) { return __one(x) * h * c * NA * 100 / 1000; }

// --- angles -----------------------------------------------------------------
// Trigonometry stays in radians, as in JavaScript; these two convert, and naming
// them is cheaper than a model silently passing degrees to Math.sin.
function radians(deg) { return __one(deg) * Math.PI / 180; }
function degrees(rad) { return __one(rad) * 180 / Math.PI; }
)JS";

constexpr int kMaxStringChars = 512;
constexpr int kMaxDepth = 4;

QVariant capped(const QVariant& value, const ScriptLimits& limits, bool* truncated, int depth)
{
    if (depth > kMaxDepth) {
        if (truncated)
            *truncated = true;
        return QStringLiteral("(nested too deeply to report)");
    }

    switch (static_cast<QMetaType::Type>(value.typeId())) {
    case QMetaType::QVariantList: {
        const QVariantList list = value.toList();
        const int keep = qMin(list.size(), qMax(0, limits.maxListValues));
        QVariantList out;
        out.reserve(keep);
        for (int i = 0; i < keep; ++i)
            out.append(capped(list.at(i), limits, truncated, depth + 1));
        if (list.size() > keep && truncated)
            *truncated = true;
        return out;
    }
    case QMetaType::QVariantMap: {
        const QVariantMap map = value.toMap();
        const int keep = qMin(map.size(), qMax(0, limits.maxListValues));
        QVariantMap out;
        int taken = 0;
        for (auto it = map.constBegin(); it != map.constEnd() && taken < keep; ++it, ++taken)
            out.insert(it.key(), capped(it.value(), limits, truncated, depth + 1));
        if (map.size() > keep && truncated)
            *truncated = true;
        return out;
    }
    case QMetaType::QString: {
        const QString text = value.toString();
        if (text.size() <= kMaxStringChars)
            return text;
        if (truncated)
            *truncated = true;
        return text.left(kMaxStringChars) + QStringLiteral("...");
    }
    default:
        return value;
    }
}

}  // namespace

QString formatNumber(double value)
{
    if (qIsNaN(value))
        return QStringLiteral("nan");
    if (qIsInf(value))
        return value > 0 ? QStringLiteral("inf") : QStringLiteral("-inf");

    QString text = QString::number(value, 'g', 10);
    // QString's 'g' keeps the exponent form where JavaScript's toPrecision does too,
    // so the two only differ in trailing zeros; strip them here as well.
    if (!text.contains(QLatin1Char('e'))) {
        if (text.contains(QLatin1Char('.'))) {
            while (text.endsWith(QLatin1Char('0')))
                text.chop(1);
            if (text.endsWith(QLatin1Char('.')))
                text.chop(1);
        }
    }
    return text;
}

const char* builtinsSource()
{
    return kBuiltins;
}

QStringList readPrints(QJSEngine& engine, int maxPrints, bool* truncated)
{
    QStringList out;
    const QJSValue collected = engine.globalObject().property(QStringLiteral("__prints"));
    if (!collected.isArray())
        return out;

    const int count = collected.property(QStringLiteral("length")).toInt();
    const int keep = qMax(0, qMin(count, maxPrints));
    out.reserve(keep);
    for (int i = 0; i < keep; ++i) {
        const QJSValue line = collected.property(static_cast<quint32>(i));
        out.append(line.isString() ? line.toString() : line.toVariant().toString());
    }
    if (count > keep && truncated)
        *truncated = true;
    return out;
}

QVariant cappedValue(const QVariant& value, const ScriptLimits& limits, bool* truncated)
{
    return capped(value, limits, truncated, 0);
}

}  // namespace script
