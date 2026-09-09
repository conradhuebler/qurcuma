// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — curcuma's atom-selection grammar, pinned.
//
// This exists because the tool layer described the grammar from a comment instead
// of from the code, and a model then followed that description into three separate
// failures: "F0" matched nothing, "0:145" produced index -1, and "F1" would have
// matched nothing either because the fragment cache had never been filled. The
// grammar is one-based; the surrounding tools are zero-based; and Fn depends on a
// call nobody would guess. All three are asserted here.

#include <src/core/molecule.h>

#include <QCoreApplication>
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

    std::printf("%s (%d failed)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
