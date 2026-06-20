// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// WP0 Qt Quick 3D spike - molecule data layer. Claude Generated.
#include "moleculedata.h"

#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QTextStream>
#include <QtMath>

namespace mol {

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

QVector<Bond> detectBonds(const QVector<Atom>& atoms)
{
    constexpr float kTolerance = 1.25f;
    QVector<Bond> bonds;
    for (int i = 0; i < atoms.size(); ++i) {
        const float ri = covalentRadius(atoms[i].element);
        for (int j = i + 1; j < atoms.size(); ++j) {
            const float maxDist = (ri + covalentRadius(atoms[j].element)) * kTolerance;
            if ((atoms[i].position - atoms[j].position).lengthSquared() <= maxDist * maxDist)
                bonds.append({ i, j, 1 });
        }
    }
    return bonds;
}

Structure makeGrid(int targetCount, float spacing)
{
    Structure s;
    // Cube side length so that side^3 just reaches targetCount.
    const int side = qMax(1, int(std::ceil(std::cbrt(double(targetCount)))));
    const float origin = -0.5f * (side - 1) * spacing; // centre the cube on (0,0,0)

    auto indexOf = [side](int x, int y, int z) { return (x * side + y) * side + z; };

    s.atoms.reserve(side * side * side);
    for (int x = 0; x < side; ++x)
        for (int y = 0; y < side; ++y)
            for (int z = 0; z < side; ++z) {
                Atom a;
                a.element = QStringLiteral("C");
                a.position = QVector3D(origin + x * spacing,
                    origin + y * spacing,
                    origin + z * spacing);
                s.atoms.append(a);
            }

    // Nearest-neighbour lattice bonds along +x/+y/+z -> exercises all three
    // cylinder orientations for the quaternion path.
    for (int x = 0; x < side; ++x)
        for (int y = 0; y < side; ++y)
            for (int z = 0; z < side; ++z) {
                const int i = indexOf(x, y, z);
                if (x + 1 < side)
                    s.bonds.append({ i, indexOf(x + 1, y, z), 1 });
                if (y + 1 < side)
                    s.bonds.append({ i, indexOf(x, y + 1, z), 1 });
                if (z + 1 < side)
                    s.bonds.append({ i, indexOf(x, y, z + 1), 1 });
            }
    return s;
}

bool loadXyz(const QString& path, Structure& out, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Cannot open %1").arg(path);
        return false;
    }
    QTextStream in(&file);
    bool ok = false;
    const int count = in.readLine().trimmed().toInt(&ok);
    if (!ok || count <= 0) {
        if (error)
            *error = QStringLiteral("Bad atom count in %1").arg(path);
        return false;
    }
    in.readLine(); // comment line

    out.atoms.clear();
    out.atoms.reserve(count);
    for (int i = 0; i < count && !in.atEnd(); ++i) {
        const QStringList tok = in.readLine().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (tok.size() < 4)
            continue;
        Atom a;
        a.element = tok[0];
        a.position = QVector3D(tok[1].toFloat(), tok[2].toFloat(), tok[3].toFloat());
        out.atoms.append(a);
    }
    out.bonds = detectBonds(out.atoms);
    return !out.atoms.isEmpty();
}

} // namespace mol
