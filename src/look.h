// look.h - Claude Generated 2026 (UX stage 3)
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
#pragma once

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <cmath>

/// A "look": how the scene is rendered (colour scheme, material, lighting, effects,
/// background) and nothing else. It deliberately carries none of the quick toggles
/// (NCI, hydrogen display, hidden molecules, labels, render style) and no
/// structure-specific setting, so applying a look cannot switch any of them
/// (MoleculeViewer::applyLook only calls the setters of the fields below).
struct Look {
    QString name;
    int colorScheme = 0;              ///< MoleculeViewer::ColorScheme as int
    float atomTransparency = 1.0f;
    float atomShininess = 80.0f;
    bool fogEnabled = false;
    float fogIntensity = 0.5f;
    float fogDistance = 0.2f;         ///< where the fog starts, 0 = near .. 1 = far
    bool ssaoEnabled = true;
    float ssaoIntensity = 1.0f;
    float ssaoRadius = 0.05f;
    float ssaoBias = 0.025f;
    bool bloomEnabled = true;
    float bloomThreshold = 0.8f;
    float bloomIntensity = 1.0f;
    bool hdrEnabled = true;
    float exposure = 1.0f;
    bool cornerLights[4] = { true, true, false, false };  ///< screen-fixed lights, see SceneController
    QColor background { 32, 36, 44 };

    /// Same appearance (name ignored); floats compared to 1e-4, so a look read back
    /// from the viewer is recognised as the one that was applied.
    bool sameAppearance(const Look& o) const
    {
        const auto eq = [](float a, float b) { return std::fabs(a - b) < 1e-4f; };
        for (int i = 0; i < 4; ++i)
            if (cornerLights[i] != o.cornerLights[i])
                return false;
        return colorScheme == o.colorScheme && eq(atomTransparency, o.atomTransparency)
            && eq(atomShininess, o.atomShininess) && fogEnabled == o.fogEnabled
            && eq(fogIntensity, o.fogIntensity) && eq(fogDistance, o.fogDistance)
            && ssaoEnabled == o.ssaoEnabled && eq(ssaoIntensity, o.ssaoIntensity)
            && eq(ssaoRadius, o.ssaoRadius) && eq(ssaoBias, o.ssaoBias)
            && bloomEnabled == o.bloomEnabled && eq(bloomThreshold, o.bloomThreshold)
            && eq(bloomIntensity, o.bloomIntensity) && hdrEnabled == o.hdrEnabled
            && eq(exposure, o.exposure) && background.rgb() == o.background.rgb();
    }
};

namespace looks {

/// The four looks that ship with qurcuma. "Default" is the factory appearance; the
/// others differ only in the fields set below.
inline QVector<Look> builtIn()
{
    QVector<Look> out;

    Look def;
    def.name = QStringLiteral("Default");
    out.append(def);

    // Light and clear for figures: white background, no fog, no bloom, subtle occlusion.
    Look pub;
    pub.name = QStringLiteral("Publication");
    pub.atomShininess = 120.0f;
    pub.ssaoIntensity = 0.8f;
    pub.bloomEnabled = false;
    pub.background = QColor(255, 255, 255);
    out.append(pub);

    // Strong on a dark background: bloom and fog for depth.
    Look pres;
    pres.name = QStringLiteral("Presentation");
    pres.atomShininess = 100.0f;
    pres.fogEnabled = true;
    pres.fogIntensity = 0.3f;
    pres.fogDistance = 0.3f;
    pres.bloomThreshold = 0.7f;
    pres.bloomIntensity = 1.2f;
    pres.exposure = 1.1f;
    pres.background = QColor(18, 20, 26);
    out.append(pres);

    // No post-processing at all: fast on weak hardware, even on a projector.
    Look flat;
    flat.name = QStringLiteral("Flat (Teaching)");
    flat.atomShininess = 40.0f;
    flat.ssaoEnabled = false;
    flat.bloomEnabled = false;
    flat.hdrEnabled = false;
    flat.background = QColor(245, 245, 245);
    out.append(flat);

    return out;
}

inline bool isBuiltInName(const QString& name)
{
    for (const Look& l : builtIn())
        if (l.name.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

inline QJsonObject toJson(const Look& l)
{
    QJsonObject o;
    o["name"] = l.name;
    o["colorScheme"] = l.colorScheme;
    o["atomTransparency"] = l.atomTransparency;
    o["atomShininess"] = l.atomShininess;
    o["fogEnabled"] = l.fogEnabled;
    o["fogIntensity"] = l.fogIntensity;
    o["fogDistance"] = l.fogDistance;
    o["ssaoEnabled"] = l.ssaoEnabled;
    o["ssaoIntensity"] = l.ssaoIntensity;
    o["ssaoRadius"] = l.ssaoRadius;
    o["ssaoBias"] = l.ssaoBias;
    o["bloomEnabled"] = l.bloomEnabled;
    o["bloomThreshold"] = l.bloomThreshold;
    o["bloomIntensity"] = l.bloomIntensity;
    o["hdrEnabled"] = l.hdrEnabled;
    o["exposure"] = l.exposure;
    QJsonArray lights;
    for (bool on : l.cornerLights)
        lights.append(on);
    o["cornerLights"] = lights;
    o["background"] = l.background.name();
    return o;
}

/// Missing keys keep the factory value, so a look saved by an older build still loads.
inline Look fromJson(const QJsonObject& o)
{
    Look l;
    l.name = o.value("name").toString();
    l.colorScheme = o.value("colorScheme").toInt(l.colorScheme);
    l.atomTransparency = float(o.value("atomTransparency").toDouble(l.atomTransparency));
    l.atomShininess = float(o.value("atomShininess").toDouble(l.atomShininess));
    l.fogEnabled = o.value("fogEnabled").toBool(l.fogEnabled);
    l.fogIntensity = float(o.value("fogIntensity").toDouble(l.fogIntensity));
    l.fogDistance = float(o.value("fogDistance").toDouble(l.fogDistance));
    l.ssaoEnabled = o.value("ssaoEnabled").toBool(l.ssaoEnabled);
    l.ssaoIntensity = float(o.value("ssaoIntensity").toDouble(l.ssaoIntensity));
    l.ssaoRadius = float(o.value("ssaoRadius").toDouble(l.ssaoRadius));
    l.ssaoBias = float(o.value("ssaoBias").toDouble(l.ssaoBias));
    l.bloomEnabled = o.value("bloomEnabled").toBool(l.bloomEnabled);
    l.bloomThreshold = float(o.value("bloomThreshold").toDouble(l.bloomThreshold));
    l.bloomIntensity = float(o.value("bloomIntensity").toDouble(l.bloomIntensity));
    l.hdrEnabled = o.value("hdrEnabled").toBool(l.hdrEnabled);
    l.exposure = float(o.value("exposure").toDouble(l.exposure));
    const QJsonArray lights = o.value("cornerLights").toArray();
    for (int i = 0; i < 4 && i < lights.size(); ++i)
        l.cornerLights[i] = lights.at(i).toBool(l.cornerLights[i]);
    const QColor bg(o.value("background").toString());
    if (bg.isValid())
        l.background = bg;
    return l;
}

} // namespace looks
