// test_bonddetect.cpp - the cell list of bonddetect.h gives the same pairs as the all-pairs loop
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "bonddetect.h"

#include <QElapsedTimer>

#include <iostream>
#include <random>

static int failures = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond << std::endl;               \
            ++failures;                                                                     \
        }                                                                                   \
    } while (0)

// The reference: every pair, same arithmetic as the former MoleculeViewer::detectBondsHysteresis.
static QVector<std::pair<int, int>> bruteForce(const QVector<QVector3D>& p, const QVector<float>& radius,
    float form, float brk, const QSet<quint64>* bonded)
{
    QVector<std::pair<int, int>> out;
    for (int i = 0; i < p.size(); ++i)
        for (int j = i + 1; j < p.size(); ++j) {
            const float distance = (p[i] - p[j]).length();
            const float r = radius[i] + radius[j];
            const float tol = (bonded && bonded->contains(bonddetect::pairKey(i, j))) ? brk : form;
            if (distance <= r * tol)
                out.append({ i, j });
        }
    return out;
}

int main()
{
    std::mt19937 rng(12345);
    const float radii[] = { 0.31f, 0.76f, 0.71f, 0.66f, 1.05f, 1.20f };  // H C N O S Br-like
    for (int trial = 0; trial < 6; ++trial) {
        const int n = trial < 5 ? 300 + 400 * trial : 2500;
        const float box = 6.0f + 4.0f * float(std::cbrt(double(n)));  // dense enough to have many pairs
        std::uniform_real_distribution<float> pos(0.0f, box), pickR(0.0f, 5.999f);
        QVector<QVector3D> p;
        QVector<float> r;
        for (int i = 0; i < n; ++i) {
            // negative coordinates exercise the cell index of atoms below the minimum of a first frame
            p.append(QVector3D(pos(rng) - box / 2, pos(rng) - box / 2, pos(rng) - box / 2));
            r.append(radii[int(pickR(rng))]);
        }
        // a previous bond set: some of the pairs found with the loose threshold
        const auto loose = bruteForce(p, r, 1.45f, 1.45f, nullptr);
        QSet<quint64> bonded;
        for (int k = 0; k < loose.size(); k += 2)
            bonded.insert(bonddetect::pairKey(loose[k].first, loose[k].second));

        const QSet<quint64>* sets[2] = { nullptr, &bonded };
        for (const QSet<quint64>* b : sets) {
            const auto want = bruteForce(p, r, 1.25f, 1.45f, b);
            const auto got = bonddetect::pairsWithin(p, r, 1.25f, 1.45f, b);
            CHECK(want == got);  // same pairs, same order
            CHECK(!want.isEmpty());
        }
    }

    // degenerate inputs
    CHECK(bonddetect::pairsWithin({}, {}, 1.25f, 1.45f, nullptr).isEmpty());
    CHECK(bonddetect::pairsWithin({ QVector3D() }, { 0.5f }, 1.25f, 1.45f, nullptr).isEmpty());
    {
        // two atoms at the same place, and two far apart
        QVector<QVector3D> p{ QVector3D(), QVector3D(), QVector3D(500, 0, 0) };
        QVector<float> r{ 0.3f, 0.3f, 0.3f };
        const auto got = bonddetect::pairsWithin(p, r, 1.25f, 1.45f, nullptr);
        CHECK(got.size() == 1 && got[0] == std::make_pair(0, 1));
    }

    // cost: 7320 atoms (the largest system in the remote log) must not take the 600 ms of the loop
    {
        const int n = 7320;
        std::uniform_real_distribution<float> pos(0.0f, 60.0f);
        QVector<QVector3D> p;
        QVector<float> r;
        for (int i = 0; i < n; ++i) {
            p.append(QVector3D(pos(rng), pos(rng), pos(rng)));
            r.append(radii[i % 4]);
        }
        QElapsedTimer t;
        t.start();
        const auto got = bonddetect::pairsWithin(p, r, 1.25f, 1.45f, nullptr);
        const double ms = double(t.nsecsElapsed()) / 1e6;
        std::cout << "7320 atoms: " << got.size() << " pairs in " << ms << " ms" << std::endl;
        CHECK(ms < 100.0);
    }

    if (failures)
        std::cerr << failures << " check(s) failed" << std::endl;
    else
        std::cout << "All checks passed." << std::endl;
    return failures ? 1 : 0;
}
