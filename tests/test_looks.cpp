// test_looks.cpp - Claude Generated 2026 (UX stage 3/4)
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Looks (src/look.h): JSON round trip over every field, factory fallback for missing
// keys, and the built-in set. A field forgotten in toJson/fromJson/sameAppearance
// shows up here instead of as a look that silently loses a setting.

#include <QColor>
#include <QJsonObject>

#include <iostream>
#include <string>

#include "look.h"

namespace {

int g_failures = 0;

void check(bool condition, const std::string& what)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << what << std::endl;
    if (!condition)
        ++g_failures;
}

} // namespace

int main()
{
    std::cout << "Look test" << std::endl;
    std::cout << "=========" << std::endl;
    Look odd;  // every field away from its default, so a field missing in toJson/fromJson shows
    odd.name = QStringLiteral("Odd");
    odd.colorScheme = 3;
    odd.atomTransparency = 0.5f;
    odd.atomShininess = 17.0f;
    odd.fogEnabled = true;
    odd.fogIntensity = 0.9f;
    odd.fogDistance = 0.7f;
    odd.ssaoEnabled = false;
    odd.ssaoIntensity = 0.3f;
    odd.ssaoRadius = 0.2f;
    odd.ssaoBias = 0.01f;
    odd.bloomEnabled = false;
    odd.bloomThreshold = 0.4f;
    odd.bloomIntensity = 2.0f;
    odd.hdrEnabled = false;
    odd.exposure = 1.7f;
    odd.cornerLights[0] = false;
    odd.cornerLights[3] = true;
    odd.background = QColor(1, 2, 3);
    check(!odd.sameAppearance(Look()), "the test look differs from the factory look");
    const Look back = looks::fromJson(looks::toJson(odd));
    check(back.sameAppearance(odd) && back.name == odd.name, "a look survives the JSON round trip");
    check(looks::fromJson(QJsonObject()).sameAppearance(Look()),
        "missing keys fall back to the factory values");
    const QVector<Look> builtIns = looks::builtIn();
    check(builtIns.size() == 4 && builtIns[0].sameAppearance(Look()),
        "four built-in looks, the first is the factory look");
    bool distinct = true;
    for (int i = 0; i < builtIns.size(); ++i)
        for (int j = i + 1; j < builtIns.size(); ++j)
            distinct = distinct && !builtIns[i].sameAppearance(builtIns[j]);
    check(distinct, "the built-in looks all differ");
    check(looks::isBuiltInName(QStringLiteral("publication")) && !looks::isBuiltInName(QStringLiteral("Odd")),
        "built-in names are recognised case-insensitively");

    std::cout << std::endl;
    if (g_failures == 0) {
        std::cout << "All checks passed." << std::endl;
        return 0;
    }
    std::cout << g_failures << " check(s) failed." << std::endl;
    return 1;
}
