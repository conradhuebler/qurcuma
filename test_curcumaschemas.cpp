// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — schemas read out of curcuma rather than written by hand.
//
// The load-bearing assertion is the first one: a generated schema has to pass the
// tool registry's own validator. If curcuma grows a parameter kind the validator
// cannot enforce, this fails here rather than by promising a model something the
// dispatcher will later reject.

#include "core/toolregistry.h"
#include "llm/curcuma_schemas.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <cstdio>

static int g_failed = 0;

static void check(bool ok, const QString& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
    if (!ok)
        ++g_failed;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // --- the drift alarm ----------------------------------------------------
    for (const QString& command : { QStringLiteral("sp"), QStringLiteral("rmsd"),
             QStringLiteral("md"), QStringLiteral("analysis") }) {
        QString error;
        const QJsonObject schema = curcumaJobSchema(command);
        check(ToolRegistry::isValidSchema(schema, &error),
            QStringLiteral("the schema generated for \"%1\" passes the validator%2")
                .arg(command, error.isEmpty() ? QString() : QStringLiteral(" [%1]").arg(error)));
    }

    // --- the command-to-module mapping is what makes this possible ----------
    {
        const QJsonObject details = curcumaJobDetails(QStringLiteral("md"));
        const QJsonArray modules = details.value(QStringLiteral("modules")).toArray();
        check(modules.size() == 1, "\"md\" resolves to exactly one module");
        const QJsonObject module = modules.isEmpty() ? QJsonObject {} : modules.first().toObject();
        check(module.value(QStringLiteral("module")).toString() == QLatin1String("simplemd"),
            "and it is simplemd -- the mapping no command name reveals");
        check(!module.value(QStringLiteral("description")).toString().isEmpty(),
            "the module says what it is, which -list_modules never did");
        check(module.value(QStringLiteral("parameter_count")).toInt() > 50,
            "and the long tail is there for describe_job to hand out on request");
    }
    {
        const QJsonObject details = curcumaJobDetails(QStringLiteral("sp"));
        const QJsonArray modules = details.value(QStringLiteral("modules")).toArray();
        check(!modules.isEmpty()
                && modules.first().toObject().value(QStringLiteral("module")).toString()
                    == QLatin1String("opt"),
            "\"sp\" resolves to the opt module, which it shares with \"opt\"");
    }

    // --- the compact schema really is compact -------------------------------
    {
        const QJsonObject properties = curcumaJobSchema(QStringLiteral("sp"))
                                           .value(QStringLiteral("properties")).toObject();
        check(!properties.isEmpty(), "the single-point schema has parameters");
        check(properties.size() <= 12,
            QStringLiteral("and stays small (%1) -- the catalogue travels with every request")
                .arg(properties.size()));
        check(properties.contains(QStringLiteral("method")), "method is offered");
        check(!properties.contains(QStringLiteral("optimizer")),
            "optimizer is not -- sp and opt share a module, and curcuma has no way to "
            "say a parameter belongs to only one of them");
    }

    // --- what used to live in prose is now in the schema --------------------
    {
        const QJsonObject properties = curcumaJobSchema(QStringLiteral("rmsd"))
                                           .value(QStringLiteral("properties")).toObject();
        const QJsonObject method = properties.value(QStringLiteral("method")).toObject();
        check(method.value(QStringLiteral("enum")).toArray().size() >= 6,
            "the RMSD alignment methods reach the schema as an enum, not as a sentence");
        check(method.value(QStringLiteral("description")).toString().contains(QStringLiteral("default:")),
            "and the default is stated");
    }
    {
        const QJsonObject properties = curcumaJobSchema(QStringLiteral("md"))
                                           .value(QStringLiteral("properties")).toObject();
        const QJsonObject temperature = properties.value(QStringLiteral("temperature")).toObject();
        check(temperature.value(QStringLiteral("description")).toString().contains(QStringLiteral("[K]")),
            "units come through");
        check(temperature.contains(QStringLiteral("minimum")), "and so do bounds");
    }

    // --- an unknown command says so -----------------------------------------
    {
        const QJsonObject details = curcumaJobDetails(QStringLiteral("not_a_command"));
        check(details.value(QStringLiteral("modules")).toArray().isEmpty(),
            "an unknown command yields no modules rather than an empty-looking answer");
    }

    std::printf("%s (%d failed)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
