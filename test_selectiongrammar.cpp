// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — curcuma's atom-selection grammar, pinned.
//
// This exists because the tool layer described the grammar from a comment instead
// of from the code, and a model then followed that description into three separate
// failures: "F0" matched nothing, "0:145" produced index -1, and "F1" would have
// matched nothing either because the fragment cache had never been filled. The
// grammar is one-based; the surrounding tools are zero-based; and Fn depends on a
// call nobody would guess. All three are asserted here.

#include "src/llm/atomselection.h"

#include <src/core/molecule.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <cstdio>
#include <string>
#include <vector>

static int g_failed = 0;

static void check(bool ok, const std::string& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok)
        ++g_failed;
}

static std::string show(const std::vector<int>& v)
{
    std::string s = "{";
    for (size_t i = 0; i < v.size(); ++i)
        s += (i ? "," : "") + std::to_string(v[i]);
    return s + "}";
}

/// The same two fragments as viewer atoms, for the helpers built on top of the
/// grammar. Kept in step with twoFragments() by hand; five atoms, one test.
static QVector<moldata::Atom> twoFragmentAtoms()
{
    const double far = 30.0;
    const auto atom = [](const char* element, double x, double y, double z) {
        moldata::Atom a;
        a.element = QString::fromLatin1(element);
        a.position = QVector3D(float(x), float(y), float(z));
        return a;
    };
    return { atom("C", 0.0, 0.0, 0.0), atom("H", 1.0, 0.0, 0.0), atom("H", 0.0, 1.0, 0.0),
             atom("O", far, 0.0, 0.0), atom("H", far + 1.0, 0.0, 0.0) };
}

/// Two fragments: three atoms at the origin, two atoms far away.
static curcuma::Molecule twoFragments()
{
    curcuma::Molecule mol;
    const double far = 30.0;
    mol.addPair({ 6, Position(0.0, 0.0, 0.0) });
    mol.addPair({ 1, Position(1.0, 0.0, 0.0) });
    mol.addPair({ 1, Position(0.0, 1.0, 0.0) });
    mol.addPair({ 8, Position(far, 0.0, 0.0) });
    mol.addPair({ 1, Position(far + 1.0, 0.0, 0.0) });
    return mol;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // --- Fn needs the fragment cache ----------------------------------------
    {
        curcuma::Molecule mol = twoFragments();
        // Deliberately NOT calling GetFragments() first.
        const std::vector<int> got = mol.FragString2Indicies("F1");
        check(got.empty(),
            "without GetFragments() the fragment cache is empty and \"F1\" matches nothing "
            "-- the trap the tool layer fell into");
    }

    curcuma::Molecule mol = twoFragments();
    const std::vector<std::vector<int>> fragments = mol.GetFragments();
    check(fragments.size() == 2, "the test molecule really has two fragments");

    // --- fragments are ONE-based --------------------------------------------
    {
        const std::vector<int> f1 = mol.FragString2Indicies("F1");
        check(f1.size() == 3, "\"F1\" is the FIRST fragment, not the second (" + show(f1) + ")");
        const std::vector<int> f2 = mol.FragString2Indicies("F2");
        check(f2.size() == 2, "\"F2\" is the second (" + show(f2) + ")");
        const std::vector<int> f0 = mol.FragString2Indicies("F0");
        check(f0.empty(),
            "\"F0\" matches nothing at all -- it is fragment -1, and it fails silently "
            "rather than reporting anything");
    }

    // --- ranges and singles are ONE-based, results are ZERO-based -----------
    {
        const std::vector<int> range = mol.FragString2Indicies("1:3");
        check(range == std::vector<int>({ 0, 1, 2 }),
            "\"1:3\" means the first three atoms and yields 0,1,2 (" + show(range) + ")");

        const std::vector<int> single = mol.FragString2Indicies("2");
        check(single == std::vector<int>({ 1 }), "\"2\" is the second atom, index 1");

        const std::vector<int> zeroBased = mol.FragString2Indicies("0:2");
        const bool hasNegative = !zeroBased.empty() && zeroBased.front() < 0;
        check(hasNegative,
            "a zero-based range produces a NEGATIVE index (" + show(zeroBased)
                + ") -- which is why every caller has to range-check the result");
    }

    // --- -1 is everything, and already zero-based ---------------------------
    {
        const std::vector<int> all = mol.FragString2Indicies("-1");
        check(all == std::vector<int>({ 0, 1, 2, 3, 4 }),
            "\"-1\" is every atom, and this one path is already zero-based");
    }

    // --- lists combine ------------------------------------------------------
    {
        const std::vector<int> combined = mol.FragString2Indicies("1,F2");
        check(combined == std::vector<int>({ 0, 3, 4 }),
            "a list mixes singles and fragments, sorted and deduplicated ("
                + show(combined) + ")");
    }

    // --- what the tools actually call ---------------------------------------
    //
    // resolveAtomSet() is the one place that turns a name into indices, and
    // severedBondCount() is what keeps a fragment energy honest: a subset that
    // cuts covalent bonds has open valences, and its energy must not be
    // subtracted from the energy of the whole.
    {
        const QVector<moldata::Atom> atoms = twoFragmentAtoms();
        QVector<int> got;
        QString error;

        check(resolveAtomSet(atoms, QStringLiteral("F1"), {}, got, error)
                && got == QVector<int>({ 0, 1, 2 }),
            "resolveAtomSet(\"F1\") fills the fragment cache first and yields 0,1,2");

        check(resolveAtomSet(atoms, QStringLiteral("F2"), {}, got, error)
                && got == QVector<int>({ 3, 4 }),
            "resolveAtomSet(\"F2\") yields the second fragment");

        check(!resolveAtomSet(atoms, QStringLiteral("F0"), {}, got, error)
                && !error.isEmpty(),
            "\"F0\" is reported as an empty match instead of silently returning nothing");

        check(!resolveAtomSet(atoms, QString(), QJsonArray({ 0, 5 }), got, error)
                && error.contains(QLatin1String("outside")),
            "an index past the end is refused by name, not passed through");

        check(!resolveAtomSet(atoms, QStringLiteral("F1"), QJsonArray({ 0 }), got, error),
            "an expression and explicit indices together are refused");

        check(subsetAtoms(atoms, QVector<int>({ 3, 4 })).size() == 2
                && subsetAtoms(atoms, QVector<int>({ 3, 4 })).at(0).element == QLatin1String("O"),
            "subsetAtoms() keeps the order it is given");

        check(severedBondCount(atoms, QVector<int>({ 0, 1, 2 })) == 0,
            "a whole fragment cuts no bond, so its energy stands on its own");
        check(severedBondCount(atoms, QVector<int>({ 3, 4 })) == 0,
            "the other whole fragment likewise cuts no bond");
        check(severedBondCount(atoms, QVector<int>({ 0, 1 })) == 1,
            "leaving one hydrogen behind cuts exactly one C-H bond");
        check(severedBondCount(atoms, QVector<int>({ 0, 1, 2, 3, 4 })) == 0,
            "selecting everything cuts nothing");

        // A fragment's index list is capped for context, which would leave a big
        // fragment impossible to name back exactly; the range is complete instead.
        check(compactRange(QVector<int>({ 0, 1, 2 })) == QLatin1String("0-2"),
            "a run of indices compacts to a range");
        check(compactRange(QVector<int>({ 3, 4 })) == QLatin1String("3-4"),
            "so does a run of two");
        check(compactRange(QVector<int>({ 5, 0, 1, 2, 9 })) == QLatin1String("0-2,5,9"),
            "gaps split it, singles stay single, and the input need not be sorted");
        check(compactRange(QVector<int>({ 7 })) == QLatin1String("7"),
            "one index is just the index");
        check(compactRange({}).isEmpty(), "and nothing is empty");
    }

    std::printf("%s (%d failed)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
