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
| Rg of the chain | tool | `analyze_trajectory` quantity gyration (streamed, block error); machine-tested only |
| RDF, H-bond count | tool | `analyze_trajectory` quantities rdf and hbonds (geometric, or by GFN-FF triple energy); opt-in log during MD with `curcuma -md -hb_log_frequency`; (`hbonds.h` in curcuma is the H4 correction, not a detector) |
| WASA (Shrake-Rupley, water probe) | tool | `analyze_trajectory` quantity sasa; the radius set of the reference study is unknown |
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
- **B. Protocol and check** (implemented: `src/llm/setupcheck.*`, `src/llm/tools_protocol.*`, tools `list_capabilities`, `check_protocol`, `save_protocol`, `protocol_stage`; `test_setupcheck` 31 checks; builds, tools not driven by a model or in the GUI): the setup as one JSON (`source`, `system`, `method`, `stages[]` with type,
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

- **D. Analysis tools** (important for the cluster workflow). Existing `Measurement` and `UnifiedAnalysis`
  load frames into memory and cannot take a pair selection for the RDF, so a streaming capability `-trajprops`
  was added to curcuma (branch `feature/trajectory-props`, `docs/TRAJPROPS.md` there).
  - Done: radius of gyration with block error; qurcuma tool `analyze_trajectory` (job via `job_status`).
    Machine-tested only (constructed geometry, 28,000 atoms x 100 frames in 3.8 s at 33 MB); real MD
    trajectories and the GUI path not tried.
  - Done: `hbonds`, geometric (distance, angle, elements adjustable; two groups per direction), also in
    `analyze_trajectory`. Counts on real water not compared with another program.
  - Done: GFN-FF triple energies on CPU and (compiled only) CUDA/ROCm; `hb_criterion energy` in `trajprops` and
    `analyze_trajectory` with a histogram for calibrating the limit. Not calibrated against the geometric count on
    real water; memory per frame grows steeply with atom count (2.1 GB at 5184 atoms, 28,000 not tried).
  - Done: opt-in logging of the energy-based count during MD (`curcuma -md -hb_log_frequency N`, CLI and cluster
    runs; not an argument of `run_simulation` / the Simulation dock). The trajectory equals the reference run
    (`scripts/md_reference_compare.py` in curcuma).
  - Done: `rdf` (two groups, element filters, orthorhombic cell) and `sasa` (Shrake-Rupley, probe 1.4 A, Bondi radii)
    in `trajprops` and `analyze_trajectory`. TODO: the literature references of all analysis methods were written by
    the AI from memory and are unchecked (curcuma `TODO.md`, `docs/TRAJPROPS.md`); the radius set Min 2015 used for
    the WASA is unknown and must be read in the paper.
  - Open, in this order: calibration on a real trajectory, water relaxation / residence times.
  - Capability inventory tool: done as `list_capabilities`.

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

## Polymer for the first run: PDMAEMA

- The polymer in curcuma's test set (`test_cases/molecules/larger/polymer.xyz`) is PEO, not PDMAEMA.
- curcuma has `src/molecules/pdmaema.xyz`, a repeat-unit fragment with two `Xx` connection points for
  `polymerbuild`. **It is not a correct PDMAEMA repeat unit.** Both `Xx` sit on the same quaternary carbon
  (C2 bears CH3, the ester and both connection points) and the fragment has no backbone CH2: it is C7H13NO2 (+2 Xx),
  the real unit is C8H15NO2. `curcuma -polymerbuild -sequence "(A)4"` with it gave C28H54N4O8 (94 atoms) instead of
  the expected C32H62N4O8, and logged bond-length retries (1.76 A against 1.50 A), "19 missing bonds, 90 extra
  bonds" and a coordination-number warning at step 02: a chain of directly bonded quaternary carbons. Checked on one
  4-mer, geometry not inspected. A fragment with -CH2-C(CH3)(CO2CH2CH2N(CH3)2)- (one `Xx` on the CH2, one on the
  quaternary carbon) is needed; check the build by the composition per unit and the topology warnings.
- Corrected unit tried (constructed by hand from `pdmaema.xyz`: one `Xx` replaced by CH2-`Xx`, tetrahedral, C-C 1.54 A;
  C8H15NO2 + 2 Xx, 28 atoms). Findings of a `(A)4` build with it: (a) the two `Xx` must be the LAST two atoms of the
  fragment file, otherwise `polymerbuild` reports an unbound atom and a bond count off by one against its template;
  (b) the 4-mer is C32H62N4O8 with the correct head-to-tail backbone (quaternary C and CH2 alternate, each quaternary
  C carries CH3 and the ester); (c) **the two chain-end cap hydrogens are not bonded** (2.6 and 2.9 A from the
  nearest carbon): `PolymerBuild::applyCapping` only changes the element of the last `Xx` and keeps its position.
  The same two unbound H appear with the original fragment, so this is a `polymerbuild` defect, not a fragment
  problem. After moving them onto the open valence and a GFN-FF optimisation (330 iterations, not converged: gradient
  norm 8e-4) the cap hydrogen on the first quaternary carbon is crowded (C-H 1.18 A, H...H 1.61 A). Not yet fixed in
  curcuma; not yet a 30-mer; the geometry was not inspected visually.
- Alternating protonation needs a second fragment (N-protonated, charge +1) and a sequence such as `(AB)15`;
  whether `polymerbuild` carries the charge into the result is not checked. Tacticity is not a parameter, it follows
  from the fragment orientation.
- The first run is a scaled-down system (about 800 to 1500 waters, 50 to 100 ps) to find where the chain breaks,
  not a reproduction of 9000 waters and 30 ns. Basis of the estimate: curcuma measured about 130 to 150 ms per MD
  step at 7320 atoms (13 Oct 2026, CPU and ROCm); 28,000 atoms were not measured. Charged multi-fragment systems
  (polymer and Cl- ions) run on the CPU method, not on the GPU (curcuma `TODO.md`).
- To the operator's knowledge the water collapse in thermostatted GFN-FF MD of a polymer in water is fixed; the
  curcuma page `docs/MD_LARGE_SYSTEMS.md` says it was not re-measured after the fix, and it has not been re-run here.

## Handover to another machine (collected, not yet done)

Nothing below has been pushed. curcuma branches, each on top of the previous one, tip `feature/rdf-sasa`:
`feature/ions` (01d62e21) -> `feature/trajectory-props` (041184f3) -> `feature/hbond-energies` (92c28a22) ->
`feature/hbond-energy-criterion` (bf18b3a3) -> `feature/md-hbond-log` (15a9ef87) -> `feature/rdf-sasa` (b4a9941f).
`feature/polymerbuild-job` is empty (master). qurcuma: `feature/paper-setup-min2015` (5a3d8e2) needs the curcuma tip to
build. To push as one development branch, `feature/rdf-sasa` can be pushed under a new name; the stacking is linear.

To collect in the vault (rules in `00 Regeln.md`): a project note for this work; method notes (hydrogen-bond criteria,
g(r), SASA, block error) with the references and the "unchecked" mark; a Labor entry for the measurements made
(reference comparison of the MD log, timings of the analysis quantities, the SASA deviation of 0.22 %); open
questions (radius set of the reference study, calibration of the energy limit, the PDMAEMA fragment, whether the
second SimpleMD run in one process has different initial velocities).

