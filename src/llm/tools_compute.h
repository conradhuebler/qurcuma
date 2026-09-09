// tools_compute.h - Tools that let the model have something calculated.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Effect::Compute, so every one of these asks before it
// runs. They start a job and return immediately: a calculation takes seconds to
// minutes, and a tool that blocks would freeze the GUI it is called from.
#pragma once

#include <QString>
#include <functional>

class CurcumaJob;
class MoleculeViewer;
class ToolRegistry;

struct ComputeToolContext {
    MoleculeViewer* viewer = nullptr;
    CurcumaJob* job = nullptr;
};

/// Register run_single_point, job_status and describe_job. Returns how many.
int registerComputeTools(ToolRegistry& registry, const ComputeToolContext& context);
