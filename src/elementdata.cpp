// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Element colour/radius tables. Claude Generated.
//
// Claude Generated 2026 - The hand-tuned display values below cover the common
// elements; everything else falls back to curcuma's full Z-indexed literature
// tables (external/curcuma/src/core/elements.h: covalent radii Pyykkö/Asumi
// 2009, vdW radii Cramer/Truhlar 2009, both in Ångström), so the builder can
// place any element with a sensible size and colour.
#include "elementdata.h"

#include "src/core/elements.h"  // curcuma Z-indexed tables (header-only)

#include <QHash>
#include <QSet>

namespace elem {

namespace {
// Symbol -> Z map built once from curcuma's abbreviation table.
const QHash<QString, int>& symbolToZ()
{
    static const QHash<QString, int> map = []() {
        QHash<QString, int> m;
        for (int z = 1; z < int(Elements::ElementAbbr.size()); ++z)
            m.insert(QString::fromStdString(Elements::ElementAbbr[z]), z);
        return m;
    }();
    return map;
}
} // namespace

int atomicNumber(const QString& symbol)
{
    return symbolToZ().value(symbol, 0);
}

QString symbolForZ(int z)
{
    if (z < 1 || z >= int(Elements::ElementAbbr.size()))
        return QString();
    return QString::fromStdString(Elements::ElementAbbr[z]);
}

QColor cpkColor(const QString& element)
{
    // Jmol-style CPK colours for the elements that commonly appear; the rest get
    // a deterministic hue from the symbol so equal elements always match.
    static const QHash<QString, QColor> colors = {
        { "H", QColor(255, 255, 255) }, { "C", QColor(128, 128, 128) },
        { "N", QColor(0, 0, 255) }, { "O", QColor(255, 0, 0) },
        { "P", QColor(255, 165, 0) }, { "S", QColor(255, 255, 0) },
        { "Cl", QColor(0, 255, 0) }, { "Br", QColor(165, 42, 42) },
        { "I", QColor(148, 0, 211) }, { "F", QColor(218, 165, 32) },
        { "Na", QColor(0, 0, 170) }, { "K", QColor(143, 124, 195) },
        { "Mg", QColor(0, 255, 0) }, { "Ca", QColor(128, 128, 144) },
        { "Fe", QColor(255, 165, 0) }, { "Zn", QColor(165, 165, 165) },
        // Claude Generated 2026 - extended Jmol CPK set for the builder.
        { "He", QColor(217, 255, 255) }, { "Li", QColor(204, 128, 255) },
        { "Be", QColor(194, 255, 0) }, { "B", QColor(255, 181, 181) },
        { "Ne", QColor(179, 227, 245) }, { "Al", QColor(191, 166, 166) },
        { "Si", QColor(240, 200, 160) }, { "Ar", QColor(128, 209, 227) },
        { "Sc", QColor(230, 230, 230) }, { "Ti", QColor(191, 194, 199) },
        { "V", QColor(166, 166, 171) }, { "Cr", QColor(138, 153, 199) },
        { "Mn", QColor(156, 122, 199) }, { "Co", QColor(240, 144, 160) },
        { "Ni", QColor(80, 208, 80) }, { "Cu", QColor(200, 128, 51) },
        { "Ga", QColor(194, 143, 143) }, { "Ge", QColor(102, 143, 143) },
        { "As", QColor(189, 128, 227) }, { "Se", QColor(255, 161, 0) },
        { "Ag", QColor(192, 192, 192) }, { "Au", QColor(255, 209, 35) },
        { "Pt", QColor(208, 208, 224) }, { "Hg", QColor(184, 184, 208) },
        { "Pb", QColor(87, 89, 97) }, { "Sn", QColor(102, 128, 128) }
    };
    const auto it = colors.constFind(element);
    if (it != colors.constEnd())
        return it.value();
    const int z = atomicNumber(element);
    if (z > 0)
        return QColor::fromHsv((z * 37) % 360, 140, 200);  // stable per element
    return QColor(200, 200, 200);
}

float vdwRadius(const QString& element)
{
    // NOTE: these are DISPLAY radii (H = 0.5 vs. the physical 1.10 Å), tuned for
    // ball-and-stick rendering. The fallback scales curcuma's physical vdW radii
    // by the same ~0.42 ratio so uncommon elements render proportionally.
    static const QHash<QString, float> radii = {
        { "H", 0.5f }, { "C", 0.7f }, { "N", 0.65f }, { "O", 0.6f },
        { "P", 1.0f }, { "S", 1.0f }, { "Cl", 1.0f }, { "Br", 1.15f },
        { "I", 1.4f }, { "F", 0.5f }, { "Na", 1.8f }, { "K", 2.2f },
        { "Mg", 1.7f }, { "Ca", 2.0f }, { "Fe", 1.4f }, { "Zn", 1.35f }
    };
    const auto it = radii.constFind(element);
    if (it != radii.constEnd())
        return it.value();
    const int z = atomicNumber(element);
    if (z > 0 && z < int(Elements::VanDerWaalsRadius.size()))
        return float(Elements::VanDerWaalsRadius[z]) * 0.42f;
    return 0.7f;
}

float covalentRadius(const QString& element)
{
    static const QHash<QString, float> radii = {
        { "H", 0.31f }, { "C", 0.76f }, { "N", 0.71f }, { "O", 0.66f },
        { "F", 0.64f }, { "P", 1.07f }, { "S", 1.05f }, { "Cl", 1.02f },
        { "Br", 1.20f }, { "I", 1.39f }, { "Na", 1.54f }, { "K", 1.96f },
        { "Mg", 1.30f }, { "Ca", 1.76f }, { "Fe", 1.32f }, { "Zn", 1.22f }
    };
    const auto it = radii.constFind(element);
    if (it != radii.constEnd())
        return it.value();
    const int z = atomicNumber(element);
    if (z > 0 && z < int(Elements::CovalentRadius.size()))
        return float(Elements::CovalentRadius[z]);
    return 0.76f;
}

bool isElementSymbol(const QString& s)
{
    // Full periodic table (H..Og). Case-sensitive: a proper symbol is a capital
    // followed by an optional lowercase, which also excludes bead labels like
    // "ppo1"/"bead1" (lowercase start) and numeric names.
    static const QSet<QString> symbols = {
        "H","He","Li","Be","B","C","N","O","F","Ne","Na","Mg","Al","Si","P","S",
        "Cl","Ar","K","Ca","Sc","Ti","V","Cr","Mn","Fe","Co","Ni","Cu","Zn","Ga",
        "Ge","As","Se","Br","Kr","Rb","Sr","Y","Zr","Nb","Mo","Tc","Ru","Rh","Pd",
        "Ag","Cd","In","Sn","Sb","Te","I","Xe","Cs","Ba","La","Ce","Pr","Nd","Pm",
        "Sm","Eu","Gd","Tb","Dy","Ho","Er","Tm","Yb","Lu","Hf","Ta","W","Re","Os",
        "Ir","Pt","Au","Hg","Tl","Pb","Bi","Po","At","Rn","Fr","Ra","Ac","Th","Pa",
        "U","Np","Pu","Am","Cm","Bk","Cf","Es","Fm","Md","No","Lr","Rf","Db","Sg",
        "Bh","Hs","Mt","Ds","Rg","Cn","Nh","Fl","Mc","Lv","Ts","Og"
    };
    return symbols.contains(s);
}

} // namespace elem
