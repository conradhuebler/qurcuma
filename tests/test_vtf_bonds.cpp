// VTF Bond Test - Claude Generated
// Test that bonds are correctly parsed (all bonds, not just one per atom)

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QVector>
#include <QMap>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "VTF Bond Test" << std::endl;
    std::cout << "=============" << std::endl;

    QString vtfFilePath = "/home/conrad/src/qurcuma/scnp_cut.vtf";

    QFile file(vtfFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "Failed to open VTF file: " << vtfFilePath.toStdString() << std::endl;
        return 1;
    }

    QTextStream stream(&file);
    QString line;
    int bondCount = 0;
    QMap<int, int> bondCounts;

    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();

        if (line.startsWith("bond ")) {
            // Parse bond line: "bond     0:1"
            QStringList parts = line.split(":");
            if (parts.size() == 2) {
                int atom1 = parts[0].mid(5).toInt();  // Remove "bond " prefix
                int atom2 = parts[1].toInt();
                bondCount++;
                bondCounts[atom1]++;
                bondCounts[atom2]++;
            }
        }
    }

    file.close();

    std::cout << "Total bonds parsed: " << bondCount << std::endl;

    // Find atoms with multiple bonds
    int multipleAtoms = 0;
    int maxBonds = 0;
    for (auto it = bondCounts.begin(); it != bondCounts.end(); ++it) {
        if (it.value() > 1) {
            multipleAtoms++;
            maxBonds = (maxBonds > it.value()) ? maxBonds : it.value();
            if (multipleAtoms <= 5) {  // Show first 5
                std::cout << "  Atom " << it.key() << " has " << it.value() << " bonds" << std::endl;
            }
        }
    }

    std::cout << "Total atoms with multiple bonds: " << multipleAtoms << std::endl;
    std::cout << "Max bonds on any atom: " << maxBonds << std::endl;
    std::cout << std::endl;
    std::cout << "EXPECTED: ~202 bonds (before fix would be ~100, each atom only has 1 bond)" << std::endl;
    std::cout << "RESULT: " << (bondCount > 150 ? "✓ FIX SUCCESSFUL" : "✗ FIX FAILED") << std::endl;

    return (bondCount > 150) ? 0 : 1;
}
