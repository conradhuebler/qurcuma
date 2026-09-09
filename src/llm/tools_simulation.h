// tools_simulation.h - Running the interactive MD and geometry optimisation.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Distinct from tools_compute.h on purpose. A single point
// is a question with an answer: it starts, finishes and hands back a number. MD and
// an optimisation are a *process* -- they push frames into the viewer, they can be
// paused, their thermostat setpoint can be moved while they run, and the geometry
// they leave behind is the new structure. That is SimulationWorker's job and it
// already does it; these tools drive the dock rather than starting a second engine.
//
// Everything goes through SimulationControlWidget, not through the worker directly.
// The dock owns the thread lifecycle, and driving its controls means the operator
// sees in the GUI what the model asked for instead of a run appearing from nowhere.
#pragma once

class MoleculeViewer;
class SimulationControlWidget;
class ToolRegistry;

struct SimulationToolContext {
    SimulationControlWidget* control = nullptr;
    /// For the element list and for a structure to measure when nothing is running.
    MoleculeViewer* viewer = nullptr;
};

/// Register the simulation tools. Returns how many.
int registerSimulationTools(ToolRegistry& registry, const SimulationToolContext& context);
