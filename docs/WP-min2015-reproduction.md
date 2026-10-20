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

## Work packages

Start point is a finished polymer XYZ (built elsewhere or by hand). Simulations may run for days on a
cluster; the analysis tools then work on the returned trajectory in a qurcuma session.

Now:

- **A. Ion placement** (curcuma `-ionize` on branch `feature/ions`, commit 01d62e21, tested on a constructed water box; qurcuma tool `add_counterions` in `src/llm/tools_edit.cpp`, builds, not run in the GUI): a capability that neutralises a solvated system
  to a target total charge with Cl-/Na+ by replacing solvent molecules (density stays fixed). Input:
  system XYZ, `charge` of the solute, ion type; solute/solvent split by connectivity. Then a qurcuma tool.
  curcuma has no solvent packing today (`fill_container` lives in qurcuma, `src/scenefiller.h`).
- **B. Protocol and check**: the setup as one JSON (`source`, `system`, `method`, `stages[]` with type,
  ensemble, temperature(s), pressure, length, time step, thermostat/barostat coupling, `analysis[]`). The
  model fills it from the prose. Tool `check_protocol` (read-only) marks each stage `ready` or `blocked`
  with the reason and the WP item, returns the arguments for `run_simulation`, `add_counterions`,
  `fill_container`, and saves `protocol.json` on request. Several temperatures are a scientific statement:
  `series: independent` (each run starts from the same state), `chained` (each starts from the end of the
  previous) or `ramp` (one run with a temperature schedule, see curcuma TEMPERATURE_RAMP.md). Whether
  independent runs execute in parallel or one after another is a resource decision of the executor, not
  part of the protocol; `check_protocol` refuses a parallel request for `chained` and `ramp`. Which
  `series` Min 2015 used is not stated in the setup summary and has to be read in the paper. A stage the stack cannot run (NPT) is accepted and marked `blocked`. The capability list
  (name, status, code location) exists once in C++ and is read by `check_protocol` and the inventory tool;
  a test checks that the named locations exist. Stage progress goes to a `status.json` in the project
  folder (written after each finished stage), so a run can stop between stages and continue from the next
  open one.
- **C. Gaps 5 and 6** written as curcuma task lists: periodic GFN-FF (non-bonded terms, long-range
  electrostatics; verified against a non-periodic reference) and a barostat (needs a variable cell in
  `SimpleMD`; scnp parser currently warns "pressure control not fully implemented").

Next:

- **D. Analysis tools** (important for the cluster workflow): `radius_of_gyration`, `rdf`, `hbonds` over a
  returned trajectory; WASA new. Capability inventory tool (reads off the same data as B).

Later:

- **F. Snakemake export**: the finished protocol (B) can be written out as a `Snakefile` plus `config.yaml`
  (one rule per stage, files in and out, temperatures as a wildcard) for running on a cluster. Depends on B.
  Points to settle then: curcuma's timestamped output directories (`-no_bmt` / `-bak`), one working
  directory per rule (a stale `stop` file aborts MD), restart of multi-day MD runs.

- **G. `curcuma -pipeline protocol.json`** (after C): curcuma runs the stages itself, writes the status
  after each, and continues with `-from <stage>`. Mid-stage restart rests on the MD `restart_file` /
  `write_restart_frequency`; that it resumes cleanly over days with thermostat and barostat is not checked.

Last:

- **E. Polymer as an offline job**: job folder with `job.json` (`_command: polymerbuild`), run by
  `curcuma -import_config job.json`, result `<name>_final.xyz`. The JSON route exists in `src/main.cpp`
  but is untried for `polymerbuild`. Branch `feature/polymerbuild-job` in `external/curcuma` is empty.
