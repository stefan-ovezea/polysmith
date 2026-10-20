# PCB test fixtures

Real fabrication data for the PCB/Gerber suites
(`cad_core_pcb_gerber_test`, `cad_core_pcb_polygonize_test`).  Committed with
the project author's permission — it is his own board design, and both Gerber
and Excellon files are plain text.

**Source:** KiCad 10.0.4 (Pcbnew) plot of the `gapstorm-retro` board —
4 copper layers, 99.9 × 99.9 mm, rendered 2026-08-22.  The full plot lives
outside the repository (see "Full board" below).

| File | What it is | Why it is here |
|---|---|---|
| `copper_trimmed.gbr` | trimmed outer copper layer (`F_Cu`) | the hard case: aperture macros, 716 pad flashes, traces, a copper pour |
| `edge_cuts.gbr` | board profile, verbatim | registration source; 10 multi-quadrant arcs |
| `paste_bottom.gbr` | bottom solder-paste layer, verbatim | flash-only layer, no regions |
| `pth.drl` | plated through-hole drill file, verbatim | Excellon: `M48` header, 13 tools, decimal coordinates |
| `npth.drl` | non-plated drill file, verbatim | Excellon degenerate case: header only, zero holes |

## How `copper_trimmed.gbr` was produced

Mechanically, from `gapstorm-retro-F_Cu.gbr` (17,058 lines), so it can be
audited or regenerated:

1. lines 1–1500 — the complete header: X2 role attributes, `%FSLAX46Y46*%`,
   `%MOMM*%`, the `RoundRect` aperture macro and **all 71 `%ADD` definitions**,
   followed by the first ~1250 path commands (all 716 pad flashes);
2. a `%LPD*%` / `G01*` reset, so the slice cannot inherit modal state;
3. lines 16765–16811 — one complete `G36`/`G37` copper pour outline;
4. `M02*`.

Result: 71 apertures, 716 flashes, 210 draws, 1 region, 35 KB.

## What the fixtures deliberately do not cover

Inch units, trailing-zero suppression, `G74` single-quadrant arcs, `%SR`
step-and-repeat, `%AB` block apertures, `%IPNEG%`, true X2 `%TF…%` / `%TA…%`
statement forms (KiCad writes them as `G04 #@!` comments), and Excellon slots
(`G85`).  Those live in the suites as synthetic cases — see
`native/cad-core/tests/pcb_gerber_test.cpp`.

## Full board

The untrimmed plot stays local.  Point `POLYSMITH_PCB_FIXTURE_DIR` at its
directory to also run the full-layer cases, which assert whole-board
properties (e.g. F_Cu → 2017 rings, 0 skipped operations, 0 diagnostics):

```bash
POLYSMITH_PCB_FIXTURE_DIR=projects/EDM pnpm test:core
```

Without it those cases print `(skipped: …)` and the suites still pass.
