# WP: Reproducing a published MD setup (Min 2015, PDMAEMA coil-to-globule)

Goal: a setup described in prose (single atactic PDMAEMA 30-mer, 9000 H2O, Cl-, PCFF, NPT 283/303/338 K,
PPPM, Nose-Hoover, 1 fs, 30 ns) is entered into the assistant, which drives the available tools and
names what is missing. Status below is checked against the code of this branch (main + curcuma master pin).
Not run on real input; no GUI check.

## Setup item -> what exists

| Setup item | Status | Where / gap |
|---|---|---|
| Load/save structure, merge | available | tools `open_structure`, `merge_structure`, `save_structure` |
| Place solvent molecules in a box to a density | available, no preequilibration | tool `fill_container` (`src/scenefiller.h`), `set_wall` with `pbc` wall |
| 9000 H2O + chain in a cubic box at 1.0 g/cm3 | partly | fill works from builder fragments; 9000 molecules not tried; no water model (TIP3P/SPC) as a force-field object |
| Build the polymer (30-mer, alternating protonation) | curcuma yes, qurcuma no tool | curcuma capability `polymerbuild` (`src/capabilities/polymerbuild.h`, params `sequence`, `fragments`, `cap`, `opt_method`); a ready polymer file can be loaded instead. No qurcuma tool wraps it; tacticity is not a parameter, it follows from the fragment files |
| Counter-ions (Cl-) for charge balance | missing, curcuma side | needs placement of ions in the solvent box in curcuma; `SimulationConfig::charge` is only a run parameter |
| Force field | decided: GFN-FF | PCFF is not needed; the comparison is GFN-FF against the paper's PCFF numbers, not a like-for-like reproduction |
| Ewald / PPPM long-range electrostatics | open: GFN-FF must be made periodic and verified | no Ewald/PME in curcuma; no minimum image in the non-bonded terms either |
| Periodic GFN-FF non-bonded terms (cutoff, images) | open: implement and verify | `wall_potential = pbc` wraps molecules but non-bonded terms are not periodic (`simplemd.h`, `wrapIntoContainer`) |
| NPT barostat | required, missing | thermostats: csvr, berendsen, andersen, nosehover, none (`simplemd.cpp`); no barostat, no cell dynamics (scnp parser warns "pressure control not fully implemented") |
| Nose-Hoover thermostat, 1 ps | available | `thermostat`, coupling time in `SimulationConfig` (via `run_simulation`) |
| 1 fs Velocity-Verlet, unconstrained bonds | available | `timestep`, `rattle off` |
| Minimisation | available | `run_simulation mode=opt` |
| 100 ps NVT then 900 ps NPT, then 3 x 30 ns | partly | NVT segment possible; NPT impossible; no staged protocol/queue tool; `set_temperature` adjusts live only |
| Rg of the chain | exists in code, no tool | `Molecule::GyrationRadius` / `GyrationRadiusPBC`, `Analysis` (`gyration.radius`) |
| RDF, H-bond count | exists in code, no tool | `Analysis` RDF settings, `hbonds.h` |
| WASA (Shrake-Rupley, water probe) | missing | only GBSA Lebedev grid in `solvation/`, no SASA output |
| Water relaxation / residence times | missing | no analysis |
| Statement of gaps by the assistant | missing | no tool returns a capability inventory; the assistant only sees tool schemas |

## Consequence

Decided: GFN-FF replaces PCFF. Blocking gaps: periodic GFN-FF (non-bonded terms, long-range
electrostatics; implemented and verified against a non-periodic reference), a barostat, ion placement.
The protocol (last row group) is an output of the assistant: a workflow list, not an executing tool.

## Work packages (proposed order)

1. **Capability inventory tool**: read-only, lists supported/unsupported setup features with a code location.
2. **Analysis tools** over existing code: `radius_of_gyration`, `rdf`, `hbonds`; WASA new.
3. **`build_polymer` tool** wrapping curcuma `polymerbuild` (sequence, fragment files, caps); alternatively load a finished file.
4. **Ion placement** in curcuma, then a tool.
5. **Protocol output**: the assistant returns the ordered workflow (minimise -> NVT -> NPT -> production,
   temperatures, lengths) as text from the setup, marking steps the stack cannot run yet.
6. **Periodic GFN-FF + verification** (curcuma; the container wall in `simplemd.h` is a stopgap).
7. **Barostat** (curcuma), after 6.
