// VTF Frame Test - Claude Generated
// Test that frames are correctly parsed from VTF file

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "VTF Frame Parsing Test" << std::endl;
    std::cout << "======================" << std::endl;

    QString vtfFilePath = "/home/conrad/src/qurcuma/scnp_cut.vtf";

    QFile file(vtfFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "Failed to open VTF file" << std::endl;
        return 1;
    }

    QTextStream stream(&file);
    QString line;
    int frameCount = 0;
    int atomCount = 0;
    int bondCount = 0;

    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();

        if (line.startsWith("atom ")) {
            atomCount++;
        } else if (line.startsWith("bond ")) {
            bondCount++;
        } else if (line.startsWith("# Start of image")) {
            frameCount++;
            std::cout << "Found frame " << (frameCount - 1) << ": " << line.toStdString() << std::endl;
        }
    }

    file.close();

    std::cout << std::endl << "VTF File Statistics:" << std::endl;
    std::cout << "  Frames found: " << frameCount << std::endl;
    std::cout << "  Atom definitions: " << atomCount << std::endl;
    std::cout << "  Bond definitions: " << bondCount << std::endl;

    if (frameCount == 0) {
        std::cerr << std::endl << "ERROR: No frames found! Parser would fail." << std::endl;
        return 1;
    }

    std::cout << std::endl << "RESULT: " << (frameCount >= 3 ? "✓ FRAME PARSING SHOULD WORK" : "✗ NOT ENOUGH FRAMES") << std::endl;

    return (frameCount >= 3) ? 0 : 1;
}
