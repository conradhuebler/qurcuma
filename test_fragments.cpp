// Fragment decomposition test - Claude Generated 2026
//
// Pins SceneController's fragment logic, which drives the host-guest tinting:
// connected components of the bond graph, ordered largest first (so fragment 0 is
// the host and stays untinted), their Hill-notation formulas, and that the tint
// only touches the non-reference fragments.

#include <QGuiApplication>
#include <QVector>

#include <iostream>

#include "scenecontroller.h"

namespace {

int g_failures = 0;

void check(bool condition, const std::string& what)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << what << std::endl;
    if (!condition)
        ++g_failures;
}

SceneController::AtomDatum atom(const char* element, float x)
{
    SceneController::AtomDatum a;
    a.element = QString::fromLatin1(element);
    a.position = QVector3D(x, 0, 0);
    return a;
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    std::cout << "Fragment decomposition test" << std::endl;
    std::cout << "===========================" << std::endl;

    // A "host" of six carbons in a ring, a "guest" of three atoms, and a lone atom.
    QVector<SceneController::AtomDatum> atoms;
    QVector<SceneController::BondDatum> bonds;

    // Guest first in file order, to prove the ordering is by size, not by position.
    atoms.append(atom("O", 10.0f));   // 0
    atoms.append(atom("H", 11.0f));   // 1
    atoms.append(atom("H", 12.0f));   // 2
    bonds.append({ 0, 1, 1 });
    bonds.append({ 0, 2, 1 });

    for (int i = 0; i < 6; ++i)       // 3..8 : the six-ring
        atoms.append(atom("C", float(i)));
    for (int i = 0; i < 6; ++i)
        bonds.append({ 3 + i, 3 + (i + 1) % 6, 1 });

    atoms.append(atom("Ar", 30.0f));  // 9 : isolated atom

    SceneController scene;
    scene.setStructure(atoms, bonds, /*keepView=*/true);

    const QVector<SceneController::FragmentInfo> fragments = scene.fragments();
    std::cout << "  fragments:";
    for (const auto& f : fragments)
        std::cout << " " << f.formula.toStdString() << "(" << f.atomCount << ")";
    std::cout << std::endl;

    check(fragments.size() == 3, "three connected components found");
    if (fragments.size() == 3) {
        check(fragments[0].atomCount == 6, "largest fragment comes first");
        check(fragments[0].formula == QStringLiteral("C6"), "ring formula is C6");
        check(fragments[1].atomCount == 3, "second fragment is the three-atom guest");
        check(fragments[1].formula == QStringLiteral("H2O"),
            "Hill notation puts H before the heteroatom when there is no carbon");
        check(fragments[2].atomCount == 1, "third fragment is the lone atom");
        check(fragments[2].formula == QStringLiteral("Ar"), "single atom formula");
    }

    // Fragment 0 is the reference and carries no tint colour; the others do.
    check(!scene.fragmentColor(0).isValid(), "the reference fragment has no tint colour");
    check(scene.fragmentColor(1).isValid(), "fragment 1 has an automatic tint colour");
    check(scene.fragmentColor(1) != scene.fragmentColor(2),
        "consecutive fragments get different hues");

    // A picked colour overrides the automatic hue; Auto restores it.
    const QColor automatic = scene.fragmentColor(1);
    scene.setFragmentColorOverride(1, QColor(0x11, 0x22, 0x33));
    check(scene.fragmentColor(1) == QColor(0x11, 0x22, 0x33), "override wins over the automatic hue");
    check(scene.hasFragmentColorOverride(1), "the override is reported as set");
    check(!scene.hasFragmentColorOverride(2), "an untouched fragment reports no override");

    // Per-fragment colour reset: an invalid colour clears just this fragment, which
    // is what the panel's "Auto" next to the swatch does. It must leave the other
    // fragments and this fragment's other properties alone.
    scene.setFragmentColorOverride(2, QColor(0x44, 0x55, 0x66));
    scene.setFragmentScaleOverride(1, 0.4f);
    scene.setFragmentColorOverride(1, QColor());
    check(!scene.hasFragmentColorOverride(1), "an invalid colour clears that fragment's override");
    check(scene.fragmentColor(1) == automatic, "the automatic hue is back");
    check(scene.hasFragmentColorOverride(2), "resetting one fragment leaves the others alone");
    check(qFuzzyCompare(scene.fragmentScaleFor(1), 0.4f),
        "resetting the colour keeps the fragment's size");

    scene.clearFragmentColorOverrides();
    check(scene.fragmentColor(1) == automatic, "clearing colour overrides restores the automatic hue");
    check(!scene.hasFragmentColorOverride(2), "clearing removes every colour override");
    scene.clearFragmentScaleOverrides();

    // Per-fragment draw scale. The reference keeps its size unless overridden, so a
    // complex opens up by shrinking the guests - and the host can be shrunk on
    // purpose to look into its cavity.
    check(qFuzzyCompare(scene.fragmentScaleFor(0), 1.0f), "reference fragment starts unscaled");
    check(qFuzzyCompare(scene.fragmentScaleFor(1), 1.0f), "guests start unscaled");
    scene.setFragmentScale(0.5f);
    check(qFuzzyCompare(scene.fragmentScaleFor(0), 1.0f),
        "the global guest scale leaves the reference alone");
    check(qFuzzyCompare(scene.fragmentScaleFor(1), 0.5f), "the global guest scale reaches guests");
    scene.setFragmentScaleOverride(0, 0.3f);
    check(qFuzzyCompare(scene.fragmentScaleFor(0), 0.3f), "the host can be shrunk by override");
    scene.setFragmentScaleOverride(1, -1.0f);
    check(qFuzzyCompare(scene.fragmentScaleFor(1), 0.5f),
        "a non-positive scale clears the override");

    // Tint strength is per fragment too, with the same default-plus-override shape,
    // so the panel's sliders always describe exactly the fragment they name.
    scene.setFragmentTint(true, 0.6f);
    check(qFuzzyCompare(scene.fragmentTintStrengthFor(0), 0.0f),
        "the reference fragment has no tint strength");
    check(qFuzzyCompare(scene.fragmentTintStrengthFor(1), 0.6f), "guests start at the default");
    scene.setFragmentTintStrengthOverride(1, 0.9f);
    check(qFuzzyCompare(scene.fragmentTintStrengthFor(1), 0.9f), "per-fragment strength override");
    check(qFuzzyCompare(scene.fragmentTintStrengthFor(2), 0.6f),
        "an override on one fragment leaves the others at the default");

    // One reset clears hue, strength and size together - the panel offers a single
    // "Reset all", so they must not fall out of step.
    scene.clearFragmentOverrides();
    check(qFuzzyCompare(scene.fragmentScaleFor(0), 1.0f), "reset restores the host size");
    check(qFuzzyCompare(scene.fragmentTintStrengthFor(1), 0.6f), "reset restores the strength");
    check(scene.fragmentColor(1) == automatic, "reset restores the automatic hue");
    scene.setFragmentScale(1.0f);

    // Bond editing has to invalidate the decomposition: cutting the ring open keeps
    // it one fragment, removing all its bonds makes six.
    QVector<SceneController::BondDatum> cut = bonds;
    cut.removeAt(cut.size() - 1);       // drop one ring bond
    scene.updateBonds(cut);
    check(scene.fragments().size() == 3, "cutting a ring open does not split it");

    QVector<SceneController::BondDatum> guestOnly;
    guestOnly.append({ 0, 1, 1 });
    guestOnly.append({ 0, 2, 1 });
    scene.updateBonds(guestOnly);
    // guest + six now-unbonded carbons + the lone argon
    check(scene.fragments().size() == 8, "removing the ring bonds frees six single carbons");

    // Claude Generated 2026 - Hydrogen display (All / Polar / None), the skeletal-formula
    // rule behind SceneController::computeHydrogenMask. Methanol, H2, a lone H and an
    // H bonded to both a carbon and the oxygen.
    std::cout << std::endl << "Hydrogen display" << std::endl;
    QVector<SceneController::AtomDatum> h;
    QVector<SceneController::BondDatum> hb;
    h.append(atom("C", 0.0f));   // 0
    h.append(atom("O", 1.0f));   // 1
    h.append(atom("H", 2.0f));   // 2  C-H
    h.append(atom("H", 3.0f));   // 3  C-H
    h.append(atom("H", 4.0f));   // 4  C-H
    h.append(atom("H", 5.0f));   // 5  O-H
    h.append(atom("H", 6.0f));   // 6  H2
    h.append(atom("H", 7.0f));   // 7  H2
    h.append(atom("H", 8.0f));   // 8  unbonded
    h.append(atom("H", 9.0f));   // 9  bonded to C and O
    hb = { { 0, 1, 1 }, { 0, 2, 1 }, { 0, 3, 1 }, { 0, 4, 1 }, { 1, 5, 1 }, { 6, 7, 1 },
           { 9, 0, 1 }, { 9, 1, 1 } };

    const auto all = SceneController::computeHydrogenMask(h, hb, SceneController::AllHydrogens);
    check(all.hiddenCount == 0, "All hides nothing");

    const auto polar = SceneController::computeHydrogenMask(h, hb, SceneController::PolarHydrogens);
    check(polar.hiddenCount == 3 && polar.hidden[2] && polar.hidden[3] && polar.hidden[4],
        "Polar hides exactly the three C-H");
    check(!polar.hidden[5], "Polar keeps the O-H");
    check(!polar.hidden[6] && !polar.hidden[7], "Polar keeps H2 (no carbon partner)");
    check(!polar.hidden[8], "Polar keeps an unbonded H");
    check(!polar.hidden[9], "Polar keeps an H with one non-carbon partner");
    check(!polar.hidden[0] && !polar.hidden[1], "heavy atoms are never hidden");

    const auto none = SceneController::computeHydrogenMask(h, hb, SceneController::NoHydrogens);
    check(none.hiddenCount == 8, "None hides every H");
    check(none.parent[5] == 1, "a hidden O-H remembers its oxygen (NCI line start)");
    check(none.parent[8] == -1, "an unbonded hidden H has no parent");

    SceneController hScene;
    hScene.setStructure(h, hb, /*keepView=*/true);
    check(!hScene.isAtomHidden(2), "the scene starts with all hydrogens shown");
    hScene.setHydrogenDisplay(SceneController::PolarHydrogens);
    check(hScene.isAtomHidden(2) && !hScene.isAtomHidden(5) && hScene.hiddenAtomCount() == 3,
        "the scene applies the Polar rule");
    QVector<SceneController::BondDatum> brokenCH = hb;
    brokenCH.removeAt(1);  // break C-H {0, 2}
    hScene.updateBonds(brokenCH);
    check(!hScene.isAtomHidden(2), "a bond change re-evaluates the mask (freed H shows)");

    // Claude Generated 2026 - Hiding molecules by kind: a C6 ring in three waters.
    std::cout << std::endl << "Hidden molecule kinds" << std::endl;
    QVector<SceneController::AtomDatum> m;
    QVector<SceneController::BondDatum> mb;
    for (int i = 0; i < 6; ++i)            // 0..5 : ring
        m.append(atom("C", float(i)));
    for (int i = 0; i < 6; ++i)
        mb.append({ i, (i + 1) % 6, 1 });
    for (int w = 0; w < 3; ++w) {           // 6..14 : three waters
        const int o = m.size();
        m.append(atom("O", 10.0f + 3 * w));
        m.append(atom("H", 11.0f + 3 * w));
        m.append(atom("H", 12.0f + 3 * w));
        mb.append({ o, o + 1, 1 });
        mb.append({ o, o + 2, 1 });
    }
    SceneController mScene;
    mScene.setStructure(m, mb, /*keepView=*/true);
    const auto kinds = mScene.moleculeKinds();
    check(kinds.size() == 2 && kinds[0].first == QStringLiteral("H2O") && kinds[0].second == 3
            && kinds[1].first == QStringLiteral("C6") && kinds[1].second == 1,
        "kinds are listed by formula, most numerous first (H2O x3, C6 x1)");
    mScene.setHiddenMoleculeKinds({ QStringLiteral("H2O") });
    check(mScene.isAtomHidden(6) && mScene.isAtomHidden(7) && mScene.isAtomHidden(14),
        "every water atom is hidden");
    check(!mScene.isAtomHidden(0) && !mScene.isAtomHidden(5), "the ring stays visible");
    check(mScene.hiddenAtomCount() == 9, "nine hidden atoms (three waters)");
    mScene.setHydrogenDisplay(SceneController::NoHydrogens);
    check(mScene.hiddenAtomCount() == 9, "an H hidden twice (kind + H display) counts once");
    mScene.setHiddenMoleculeKinds({});
    check(!mScene.isAtomHidden(6) && mScene.isAtomHidden(7),
        "Show All brings the waters back; the H display still applies");

    // Claude Generated 2026 - The same rule as a pure function, used by RMSD overlays:
    // the guest/host/argon system above, hiding the water guest.
    const SceneController::FragmentSplit split = SceneController::computeFragments(atoms, bonds);
    check(split.info.size() == 3 && split.info[1].formula == QStringLiteral("H2O")
            && split.fragmentOf[0] == 1 && split.fragmentOf[3] == 0 && split.fragmentOf[9] == 2,
        "computeFragments gives the cached decomposition (largest first, formulas)");
    const QVector<bool> kindMask =
        SceneController::computeMoleculeKindMask(atoms, bonds, { QStringLiteral("H2O") });
    check(kindMask.size() == atoms.size() && kindMask[0] && kindMask[1] && kindMask[2]
            && !kindMask[3] && !kindMask[8] && !kindMask[9],
        "the molecule-kind mask hides exactly the water's three atoms");
    check(!SceneController::computeMoleculeKindMask(atoms, bonds, {}).contains(true),
        "no hidden kinds, nothing hidden");

    // A pi-stacking line runs between ring centroids (no end atom); its owner atoms tie
    // it to the ring (0) and to a water (6), so hiding waters hides the line too.
    SceneController::NciSegment stack;
    stack.a = QVector3D(2.5f, 0.0f, 0.0f);
    stack.b = QVector3D(11.0f, 0.0f, 0.0f);
    stack.ownerA = 0;
    stack.ownerB = 6;
    stack.label = QStringLiteral("stack");
    mScene.setNciLabelsVisible(true);
    mScene.setNciContacts({ stack });
    mScene.setNciVisible(true);
    check(mScene.nciLabels().size() == 1, "the pi-stacking line is drawn while waters are shown");
    mScene.setHiddenMoleculeKinds({ QStringLiteral("H2O") });
    check(mScene.nciLabels().isEmpty(), "hiding the waters also drops the pi-stacking line that touches one");
    mScene.setHiddenMoleculeKinds({});


    std::cout << std::endl;
    if (g_failures == 0) {
        std::cout << "All checks passed." << std::endl;
        return 0;
    }
    std::cout << g_failures << " check(s) failed." << std::endl;
    return 1;
}
