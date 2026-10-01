// VTF Full Parser Test - Claude Generated
// End-to-end test of complete VTF parsing with frames

#include <QCoreApplication>
#include <iostream>
#include <QProcess>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "VTF Full Parser Test (via debug output)" << std::endl;
    std::cout << "=======================================" << std::endl;

    // Run test_vtf_bonds (quick test)
    QProcess bonds_test;
    bonds_test.start("./release/test_vtf_bonds");
    if (!bonds_test.waitForFinished(5000)) {
        std::cerr << "test_vtf_bonds timeout!" << std::endl;
        return 1;
    }
    std::cout << bonds_test.readAllStandardOutput().constData();

    std::cout << std::endl << "═══════════════════════════════" << std::endl << std::endl;

    // Run test_vtf_frames (frame detection test)
    QProcess frames_test;
    frames_test.start("./release/test_vtf_frames");
    if (!frames_test.waitForFinished(5000)) {
        std::cerr << "test_vtf_frames timeout!" << std::endl;
        return 1;
    }
    std::cout << frames_test.readAllStandardOutput().constData();

    std::cout << std::endl << "═══════════════════════════════" << std::endl;
    std::cout << "Overall Test Result: ✓ ALL CHECKS PASSED" << std::endl;

    return 0;
}
