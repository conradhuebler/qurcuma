// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// scenefiller — pack N copies of builder fragments into a container (sphere or
// box) at random positions and orientations, keeping a minimum distance to
// everything already there. Used to set up gas-phase reaction scenes (N2 + H2
// for ammonia synthesis, a mixture of small molecules for prebiotic chemistry)
// that would be tedious to place by hand.
//
// Free functions over plain data, no Qt widgets, so the packing is testable
// without a GUI (test_scenefiller). Claude Generated 2026.
#pragma once

#include "fragmentlibrary.h"
#include "view.h"

#include <QQuaternion>
#include <QRandomGenerator>
#include <QVector>
#include <QVector3D>

namespace build {

/** @brief One entry of a fill request: how many copies of which fragment. */
struct FillRequest {
    const Fragment* fragment = nullptr;
    int count = 0;
};

/** @brief The volume the copies are packed into. Coordinates are the viewer's
 *  intrinsic frame, which is also curcuma's wall frame: a spherical wall is
 *  measured from the origin. */
struct Container {
    enum Kind { Sphere,
        Box };
    Kind kind = Sphere;
    float radius = 6.0f;                        // Sphere: radius in Angstrom
    QVector3D min { -6, -6, -6 }, max { 6, 6, 6 }; // Box: bounds in Angstrom
    /// Every atom stays this far inside the boundary, so a molecule does not start
    /// out already pressed against the wall potential.
    float margin = 0.6f;
};

/** @brief Outcome of a fill. @c placed may fall short of @c requested when the
 *  container is too small for the requested number of copies. */
struct FillResult {
    QVector<MoleculeViewer::Atom> atoms;
    QVector<MoleculeViewer::Bond> bonds;
    int placed = 0;
    int requested = 0;
};

/** @brief True when @p p lies inside the container, keeping @c margin clear. */
bool insideContainer(const Container& c, const QVector3D& p);

/** @brief Uniformly distributed random rotation (Shoemake's method). */
QQuaternion randomRotation(QRandomGenerator& rng);

/** @brief Uniformly distributed random point inside the container (rejection
 *  sampling in the bounding box; the margin is already respected). */
QVector3D randomPointIn(const Container& c, QRandomGenerator& rng);

/**
 * @brief Pack the requested fragment copies into the container.
 *
 * Each copy is randomly rotated about its centroid and placed at a random point.
 * A copy is rejected and retried when any of its atoms would leave the container
 * or come closer than @p minDistance to an atom already present (either from
 * @p existing or from a copy placed earlier in this call). After
 * @p maxAttemptsPerCopy failures that copy is skipped, so a container that is
 * too small yields @c placed < @c requested rather than looping forever.
 *
 * Fragment bonds are carried over with re-indexed atom numbers, so bond orders
 * (N2 triple, O2 double) survive into the scene.
 *
 * @param seed 0 draws from the global generator; any other value makes the
 *        packing reproducible.
 */
FillResult fillContainer(const QVector<FillRequest>& requests,
    const Container& container,
    const QVector<MoleculeViewer::Atom>& existing = {},
    float minDistance = 2.2f,
    int maxAttemptsPerCopy = 2000,
    quint32 seed = 0);

} // namespace build
