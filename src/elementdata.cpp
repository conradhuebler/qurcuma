// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Element colour/radius tables. Claude Generated.
#include "elementdata.h"

#include <QHash>
#include <QSet>

namespace elem {

QColor cpkColor(const QString& element)
{
    static const QHash<QString, QColor> colors = {
        { "H", QColor(255, 255, 255) }, { "C", QColor(128, 128, 128) },
        { "N", QColor(0, 0, 255) }, { "O", QColor(255, 0, 0) },
        { "P", QColor(255, 165, 0) }, { "S", QColor(255, 255, 0) },
        { "Cl", QColor(0, 255, 0) }, { "Br", QColor(165, 42, 42) },
        { "I", QColor(148, 0, 211) }, { "F", QColor(218, 165, 32) },
        { "Na", QColor(0, 0, 170) }, { "K", QColor(143, 124, 195) },
        { "Mg", QColor(0, 255, 0) }, { "Ca", QColor(128, 128, 144) },
        { "Fe", QColor(255, 165, 0) }, { "Zn", QColor(165, 165, 165) }
    };
    return colors.value(element, QColor(200, 200, 200));
}

float vdwRadius(const QString& element)
{
    static const QHash<QString, float> radii = {
        { "H", 0.5f }, { "C", 0.7f }, { "N", 0.65f }, { "O", 0.6f },
        { "P", 1.0f }, { "S", 1.0f }, { "Cl", 1.0f }, { "Br", 1.15f },
        { "I", 1.4f }, { "F", 0.5f }, { "Na", 1.8f }, { "K", 2.2f },
        { "Mg", 1.7f }, { "Ca", 2.0f }, { "Fe", 1.4f }, { "Zn", 1.35f }
    };
    return radii.value(element, 0.7f);
}

float covalentRadius(const QString& element)
{
    static const QHash<QString, float> radii = {
        { "H", 0.31f }, { "C", 0.76f }, { "N", 0.71f }, { "O", 0.66f },
        { "F", 0.64f }, { "P", 1.07f }, { "S", 1.05f }, { "Cl", 1.02f },
        { "Br", 1.20f }, { "I", 1.39f }, { "Na", 1.54f }, { "K", 1.96f },
        { "Mg", 1.30f }, { "Ca", 1.76f }, { "Fe", 1.32f }, { "Zn", 1.22f }
    };
    return radii.value(element, 0.76f);
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
