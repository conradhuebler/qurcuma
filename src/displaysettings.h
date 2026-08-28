// displaysettings.h - Shared display/appearance fields for the viewer.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Single source of truth for the display settings that
// were previously redeclared field-by-field in both Settings::VisualizationSettings
// and ViewPreset. Those two copies had already drifted apart; keeping the shared
// fields (and their default values) in one place stops that. Both structs inherit
// this base, so every existing `.renderingMode` / `.bondThickness` / ... access
// keeps working unchanged — only the declarations move here.

#pragma once

#include <QtGlobal>  // qreal

/** @brief The display/appearance state common to the live viewer settings and to
 *  a saved view preset. Values here are the canonical defaults. Structs that need
 *  extra fields (instancing threshold, camera, corner lights, ...) inherit this.
 */
struct DisplaySettings {
    int renderingMode = 0;        // MoleculeViewer::RenderingMode enum value
    int colorScheme = 0;          // MoleculeViewer::ColorScheme enum value
    float atomTransparency = 1.0f;
    float atomShininess = 80.0f;
    float atomScaleFactor = 1.0f;
    float bondThickness = 0.15f;
    bool fogEnabled = false;
    float fogIntensity = 0.5f;
    // Post-processing effects.
    bool ssaoEnabled = true;
    float ssaoIntensity = 1.0f;
    float ssaoRadius = 0.05f;
    float ssaoBias = 0.025f;
    bool bloomEnabled = true;
    float bloomThreshold = 0.8f;
    float bloomIntensity = 1.0f;
    bool hdrEnabled = true;
    float exposure = 1.0f;
    int rotationMode = 0;         // MoleculeViewer::RotationMode (0 = Model, 1 = CameraOrbit)
    bool wallVisible = true;      // confinement-wall wireframe show/hide override
    qreal wallOpacity = 0.6;      // wireframe alpha 0..1
    // Claude Generated 2026 - Non-covalent interaction overlay.
    int nciSource = 0;            // 0=off, 1=geometry, 2=GFN-FF parameters, 3=population
    bool nciHydrogenBonds = true;
    bool nciHalogenBonds = true;
    bool nciPiStacking = true;
    bool nciCloseContacts = false;
    bool nciElectrostatics = false;  // GFN-FF source only
    bool nciDispersion = false;      // GFN-FF source only
    float nciHbDistance = 2.50f;  // H...A cutoff [Angstrom]
    float nciHbAngle = 130.0f;    // D-H...A cutoff [degrees]
    bool nciLabels = true;        // draw the interaction distance at each contact
    bool nciLiveMd = false;       // keep the GFN-FF contact list live during MD
    // Claude Generated 2026 - Fragment tinting (host-guest systems).
    bool fragmentTint = false;
    float fragmentTintStrength = 0.6f;
    float fragmentScale = 1.0f;   // draw scale of the non-reference fragments
};
