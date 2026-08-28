// ncianalysis.h - Non-covalent interaction (NCI) detection
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026
//
// Geometric detection of non-covalent contacts in a molecular structure:
// hydrogen bonds, halogen bonds, pi-stacking and generic van-der-Waals contacts.
// The criteria are plain distance/angle tests, deliberately kept in one flat file
// so the chemistry stays readable; every threshold carries its literature source.
//
// The result types live in ncitypes.h; the same Contact/Result also carry results
// that come from a GFN-FF parameter set (see ncianalysisworker.h), so the overlay
// and the table do not care which source produced a row.

#pragma once

#include <QString>
#include <QVector>

#include "ncitypes.h"
#include "view.h"

namespace nci {


/// Aromatic-ring perception (topology only, coordinate independent): five- and
/// six-membered rings in which every atom has at most three bonded neighbours
/// (an sp2 proxy). Planarity is a geometric property and is therefore checked
/// per frame inside detectGeometric(). Claude Generated.
QVector<QVector<int>> findAromaticRings(int atomCount, const QVector<MoleculeViewer::Bond>& bonds);

/// Detect non-covalent contacts by distance and angle criteria.
///
/// Hydrogen bond D-H...A: D in {N,O,F,S}, A in {N,O,F,S,Cl,Br,I}, H covalently
/// bound to D, d(H...A) <= Options::hbMaxDistance, angle(D-H...A) >= hbMinAngle.
/// Defaults 2.50 A / 130 deg cover Jeffrey's strong and moderate bands plus the
/// top of the weak band (Jeffrey, An Introduction to Hydrogen Bonding, OUP 1997,
/// Tab. 2.1); the angular gate is what the IUPAC definition requires to separate
/// a hydrogen bond from an accidental proximity (Arunan et al., Pure Appl. Chem.
/// 2011, 83, 1637).
///
/// Halogen bond C-X...A: X in {Cl,Br,I,At} - fluorine is excluded because it has
/// no sigma-hole (Politzer, Murray, Clark, Phys. Chem. Chem. Phys. 2010, 12,
/// 7748). d(X...A) <= 0.95 * sum of van der Waals radii, angle(C-X...A) >= 150
/// deg, since the sigma-hole lies on the extension of the R-X axis (Desiraju et
/// al., Pure Appl. Chem. 2013, 85, 1711).
///
/// Pi-stacking: planar five/six-rings with centroid distance <= 5.5 A, either
/// parallel (interplanar angle <= 30 deg and lateral offset <= 2.0 A) or T-shaped
/// (interplanar angle >= 60 deg). Janiak, J. Chem. Soc. Dalton Trans. 2000, 3885;
/// Martinez and Iverson, Chem. Sci. 2012, 3, 2191.
///
/// Close contact: d(i,j) <= 0.90 * sum of van der Waals radii, no directionality.
/// A separation below the van der Waals sum marks a specific rather than an
/// accidental contact (Bondi, J. Phys. Chem. 1964, 68, 441; Alvarez, Dalton
/// Trans. 2013, 42, 8617).
///
/// @param rings Optional pre-computed aromatic rings from findAromaticRings().
///              Pass the cached list to avoid re-running ring perception on every
///              frame; nullptr makes the function compute them itself.
/// Claude Generated.
QVector<Contact> detectGeometric(const QVector<MoleculeViewer::Atom>& atoms,
    const QVector<MoleculeViewer::Bond>& bonds,
    const Options& opt,
    const QVector<QVector<int>>* rings = nullptr);

/// Recompute distances, angles and scores of existing contacts against new
/// coordinates. Used when the frame changes while a GFN-FF result is displayed:
/// the contact pairs stay, only their geometry is refreshed. Claude Generated.
void refreshGeometry(QVector<Contact>& contacts,
    const QVector<MoleculeViewer::Atom>& atoms,
    const Options& opt);

/// Drop directional contacts that are not geometrically engaged in this frame.
///
/// GFN-FF enumerates every plausible A-H...B and A-X...B triple it might ever have
/// to evaluate - thousands of them for a medium-sized molecule - and lets its
/// damping functions decide how much each contributes. Drawing all of them says
/// nothing. This applies the same distance and angle gate the geometric source
/// uses, so the force-field source shows the terms that are actually engaged in
/// the displayed frame. Call refreshGeometry() first. Claude Generated.
void applyGeometricGate(QVector<Contact>& contacts,
    const QVector<MoleculeViewer::Atom>& atoms,
    const Options& opt);

/// Sort by interaction class, then by descending score, and cut the list to
/// Options::maxContacts. Shared by the geometric and the calculated sources so
/// both honour the same hard cap.
/// @return how many contacts were dropped, so a caller can say so instead of
///         presenting a truncated list as complete. Claude Generated.
int rankAndTruncate(QVector<Contact>& contacts, const Options& opt);

/// Keep at most @p limit contacts of one interaction class, the strongest first.
/// Used for the GFN-FF pair terms, which otherwise crowd out every other class.
/// @return how many were dropped. Claude Generated.
int limitKind(QVector<Contact>& contacts, Kind kind, int limit);

/// Append a note about contacts left out of the list. Empty when @p dropped is 0.
/// Claude Generated.
QString truncationNote(int dropped);

/// Physical van der Waals radius in Angstrom (Cramer and Truhlar, J. Phys. Chem.
/// A 2009, 113, 5806), taken from curcuma's element tables.
///
/// Deliberately NOT elem::vdwRadius(): that one is a ball-and-stick DRAW radius
/// (H = 0.5 A) covering 16 elements, so a "sum of van der Waals radii" criterion
/// built on it would be wrong by more than a factor of two.
/// Returns 0 for labels that are not element symbols (coarse-grained beads).
/// Claude Generated.
double vdwRadius(const QString& element);

bool isHydrogenBondDonor(const QString& element);     ///< N, O, F, S
bool isHydrogenBondAcceptor(const QString& element);  ///< N, O, F, S, Cl, Br, I
bool isHalogenDonor(const QString& element);          ///< Cl, Br, I, At (not F)
bool isLewisBase(const QString& element);             ///< N, O, F, S, Se, P, Cl, Br, I

/// One-line description used in the contacts table, e.g. "N7-H8...O12".
/// Claude Generated.
QString describe(const Contact& c, const QVector<MoleculeViewer::Atom>& atoms);

} // namespace nci
