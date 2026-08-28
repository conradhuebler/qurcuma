// ncitypes.h - Data types of the non-covalent interaction analysis
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026
//
// Split out of ncianalysis.h so view.h can hold an nci::Options / nci::Result
// member without including the detector (which in turn needs view.h for the
// atom and bond types).

#pragma once

#include <QColor>
#include <QHash>
#include <QMetaType>
#include <QPair>
#include <QString>
#include <QVector>

namespace nci {

/// Interaction classes. HydrogenBond/HalogenBond mirror GFN-FF's own three-body
/// terms, so the geometric and the force-field source produce comparable rows.
/// Electrostatic and Dispersion only ever come from the GFN-FF parameter set.
enum class Kind {
    HydrogenBond,
    HalogenBond,
    PiStacking,
    CloseContact,
    Electrostatic,
    Dispersion
};

/// Where a Result came from. The numeric values are persisted in DisplaySettings
/// (0 additionally means "overlay off"), so do not renumber.
enum class Source {
    Geometry = 1,
    GfnffParameters = 2,
    Population = 3
};

/// A single non-covalent contact.
///
/// Index convention follows curcuma's GFNFFHydrogenBond {i,j,k}: @c donor is the
/// heavy donor atom (D in D-H...A, C in C-X...A), @c bridge is the hydrogen or the
/// halogen, @c acceptor is the acceptor. Two-body contacts leave @c bridge at -1.
/// Pi-stacking instead fills @c ringA / @c ringB and reports the ring atom closest
/// to each centroid in @c donor / @c acceptor, so selection and the table still work.
struct Contact {
    Kind kind = Kind::HydrogenBond;
    int donor = -1;
    int bridge = -1;
    int acceptor = -1;
    QVector<int> ringA;      ///< pi-stacking only: atom indices of the first ring
    QVector<int> ringB;      ///< pi-stacking only: atom indices of the second ring
    float distance = 0.0f;   ///< H...A, X...A, centroid-centroid or i...j [Angstrom]
    float angle = 0.0f;      ///< D-H...A, C-X...A or interplanar angle [degrees]
    float offset = 0.0f;     ///< pi-stacking only: lateral centroid offset [Angstrom]
    float score = 0.0f;      ///< 0..1 display score (line width/alpha) - not an energy
    double energy = 0.0;     ///< pair energy [kJ/mol], only meaningful if hasEnergy
    bool hasEnergy = false;
    int motif = 0;           ///< GFN-FF case_type (1..4); 0 for the geometric source
};

/// Detection switches and thresholds. The defaults are the values documented at
/// nci::detectGeometric(); the UI exposes the two hydrogen-bond numbers only.
struct Options {
    bool hydrogenBonds = true;
    bool halogenBonds = true;
    bool piStacking = true;
    bool closeContacts = false;  ///< off by default: floods folded structures
    /// GFN-FF source only: evaluate the Coulomb pair list (real per-pair energies).
    bool electrostatics = false;
    /// GFN-FF source only: evaluate the D4 dispersion pair list (real per-pair energies).
    bool dispersion = false;

    float hbMaxDistance = 2.50f;   ///< H...A cutoff [Angstrom]
    float hbMinAngle = 130.0f;     ///< D-H...A cutoff [degrees]
    float xbVdwFraction = 0.95f;   ///< X...A <= fraction * (rvdw_X + rvdw_A)
    float xbMinAngle = 150.0f;     ///< C-X...A cutoff [degrees]

    float piMaxCentroid = 5.5f;    ///< centroid-centroid cutoff [Angstrom]
    float piParallelAngle = 30.0f; ///< interplanar angle for parallel stacking [degrees]
    float piTShapeAngle = 60.0f;   ///< interplanar angle above which T-shaped counts
    float piMaxOffset = 2.0f;      ///< lateral offset for parallel stacking [Angstrom]
    float piPlanarityRms = 0.10f;  ///< max RMS deviation from the ring plane [Angstrom]

    float contactVdwFraction = 0.90f;
    bool includeHH = false;        ///< include H...H in the close-contact scan

    int minBondSeparation = 3;     ///< 3 = skip 1-2 and 1-3 pairs; 4 = also skip 1-4
    int fragmentFilter = 0;        ///< 0 = all, 1 = intermolecular only, 2 = intramolecular only
    int maxContacts = 400;         ///< hard cap on the returned list
};

/// One analysis result: the contacts of a single frame plus a one-line summary
/// for the panel header.
struct Result {
    Source source = Source::Geometry;
    int frame = 0;
    QVector<Contact> contacts;
    QString summary;
};

/// Atoms that make up a contact, for selecting it in the 3D view. Claude Generated.
QVector<int> contactAtoms(const Contact& c);

/// Short human-readable interaction name ("Hydrogen bond", ...). Claude Generated.
QString kindName(Kind k);

/// Default overlay colour for an interaction class. Electrostatic contacts are
/// coloured by the sign of @p energy (attractive blue, repulsive red).
/// Claude Generated.
QColor kindColor(Kind k, double energy = 0.0);

/// User-chosen overlay colours, keyed by paletteKey(). A missing or invalid entry
/// falls back to the default colour above. Claude Generated.
using Palette = QHash<int, QColor>;

/// Palette key for repulsive electrostatic pairs. The default palette splits the
/// electrostatic term by sign, so it needs a second key to stay overridable
/// without losing that distinction. Claude Generated.
constexpr int ElectrostaticRepulsiveKey = 100;

/// Palette key of a contact: int(Kind), except repulsive electrostatics.
/// Claude Generated.
int paletteKey(Kind k, double energy);

/// Overlay colour honouring @p palette. Claude Generated.
QColor kindColor(Kind k, double energy, const Palette& palette);

/// Every entry a colour selector should offer: palette key plus its label, in a
/// stable order. Claude Generated.
QVector<QPair<int, QString>> paletteEntries();

/// Summary line for the panel header, e.g. "Geometry: 7 hydrogen bonds, 1 halogen
/// bond". Claude Generated.
QString summarize(const QVector<Contact>& contacts, Source source);

} // namespace nci

Q_DECLARE_METATYPE(nci::Contact)
Q_DECLARE_METATYPE(nci::Result)
