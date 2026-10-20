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
| **Build the polymer (30-mer, atactic, alternating protonation)** | **missing** | builder has `add_fragment`/`add_atoms` only; no monomer/repeat-unit chain builder, no tacticity, no protonation-state pattern |
| Counter-ions (Cl-) for charge balance | missing | no ion placement tool; total `charge` is a run parameter (`SimulationConfig::charge`) but nothing computes or neutralises it |
| **Force field PCFF** (class II, water included) | **missing** | native FFs: GFN-FF, UFF, UFF-D3, QMDFF, CG-LJ (`method_factory.cpp`). No PCFF/CVFF/OPLS/AMBER/CHARMM, no atom typing, no parameter files |
| Ewald / PPPM long-range electrostatics (RMS 1e-4) | missing | no Ewald/PME in curcuma; no minimum image in the non-bonded terms either |
| 9.5 A pair cutoff with periodic images | missing | `wall_potential = pbc` wraps molecules but non-bonded terms are not periodic (`simplemd.h`, `wrapIntoContainer`) |
| NPT: Nose-Hoover barostat, 2 ps | missing | thermostats: csvr, berendsen, andersen, nosehover, none (`simplemd.cpp`); no barostat, no cell dynamics (scnp parser warns "pressure control not fully implemented") |
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

The run itself cannot be reproduced with this stack: three blocking gaps (PCFF, periodic long-range
electrostatics, barostat) plus the polymer builder. What can be done today: build the system geometry
(once a chain builder exists), pack water to density, and run NVT with a GFN-FF/UFF-class force field
for a qualitative comparison, which is not a reproduction of the paper.

## Work packages (proposed order)

1. **Capability inventory tool** (small): a read-only tool that lists supported/unsupported setup features,
   each tied to a code location, so the assistant reports gaps instead of guessing. Needs a single source
   of truth to avoid going stale.
2. **Analysis tools** over existing code: `radius_of_gyration`, `rdf`, `hbonds` on a trajectory; WASA new.
3. **Polymer builder**: repeat-unit library (SMILES or fragment with head/tail atoms), chain length,
   tacticity (meso/racemo pattern), per-unit protonation pattern, extended starting conformation; tool + GUI.
4. **Ion placement**: neutralise to a target charge with Cl-/Na+ in the solvent box.
5. **Staged protocol tool**: minimise -> NVT -> NPT -> production as one declarative sequence.
6. **Periodic non-bonded + Ewald/PME + barostat** (large, curcuma side; the container wall in `simplemd.h` is a stopgap).
7. **PCFF** (large, curcuma side): atom typing, class II terms, parameter set with licence check.
