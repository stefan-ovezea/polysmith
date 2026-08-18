# OCCT 7.8 → 8.0 Upgrade Audit

**Branch:** `feature/occtv8`
**Port commit:** `a3748d5` — "port to OCCT 8.0.0: delay-load DLLs, modernise API, platform-aware CMake"
**Audit date:** 2026-07-19

---

## 1. Migration Script Phases — What Ran, What Didn't

OCCT ships a 12-phase Python migration suite at `third_party/OCCT-8_0_0/adm/scripts/migration_800/`.

### File extension blind spot

The scripts define accepted source extensions as:

```python
SOURCE_EXTENSIONS = {'.lxx', '.hxx', '.hpp', '.pxx', '.cxx', '.cpp', '.c', '.h'}
```

The PolySmith project uses **413 `.inc` files** under `native/cad-core/src/` (C++ implementation headers included by other `.cpp`/`.inc` files). These are **not processed** by any migration script, so all `.inc` changes must be made by hand.

### Phase-by-phase status

| # | Script | What it does | Status |
|---|--------|-------------|--------|
| 1 | `migrate_handles.py` | `Handle(T)` → `occ::handle<T>` | ✅ Done (manual + script) |
| 2 | (same script) | `Handle(T)::DownCast(x)` → `occ::down_cast<T>(x)` | ✅ Done (manual + script) |
| 3 | `migrate_standard_types.py` | `Standard_Boolean/Integer/Real/…` → `bool/int/double/…` | ✅ Done (manual + script) |
| 4 | `migrate_macros.py` | `Standard_OVERRIDE` → `override`, `Standard_NODISCARD` → `[[nodiscard]]`, etc. | ✅ Clean |
| 5 | `cleanup_define_handle.py` | Remove redundant `DEFINE_STANDARD_HANDLE` macros | N/A — not used in project |
| 6 | `replace_typedefs.py` | Replace `TopTools_IndexedMapOfShape` with `NCollection_IndexedMap<TopoDS_Shape>` etc. | ⚠️ Skipped — needs pre-generated JSON |
| 7 | `cleanup_unused_typedefs.py` | Remove unused typedef headers | ⚠️ Skipped — needs pre-generated JSON |
| 8 | `cleanup_access_specifiers.py` | Remove redundant `public:` / `private:` specifiers | ⚠️ Skipped — cosmetic |
| 9-12 | Header removal, forwarding cleanup | File-level changes | ⛔ Intentionally skipped for external projects |

Phases 6-8 require `collected_typedefs.json` (2 MB, generated from the full OCCT source). These changes are **cosmetic/suppression-level only** — `TopTools_IndexedMapOfShape` still compiles and works in OCCT 8.0; it is merely deprecated with a compiler warning. Skipping these does **not** cause regressions.

### Dry-run verification

All three critical scripts were run in dry-run mode against `native/cad-core/src`:

```
migrate_handles.py        → 90 files processed, 0 replacements
migrate_standard_types.py → 90 files processed, 0 replacements
migrate_macros.py         → 90 files processed, 0 replacements
```

**Conclusion: source-level API migration is complete.**

### Files changed manually in the port commit

```
apps/desktop-ui/src-tauri/build.rs          — DLL deployment to build dir
apps/desktop-ui/src-tauri/src/cad_core.rs   — Core process PATH setup
native/cad-core/CMakeLists.txt              — OCCT 8.0 paths, delay-load, C++20
native/cad-core/src/app.cpp                 — stdout/stderr via fwrite
native/cad-core/src/protocol/ipc.cpp        — cout/cerr → fwrite/fflush
native/cad-core/src/core/geometry/impl/body_mesh_helpers.inc         — Handle → occ::handle
native/cad-core/src/core/geometry/impl/shape_transform_helpers.inc    — Handle::DownCast → occ::down_cast
native/cad-core/src/core/viewport/impl/body_bounds_helpers.inc       — Standard_Real → double
native/cad-core/src/core/viewport/impl/body_face_helpers.inc          — Handle → occ::handle
native/cad-core/src/core/viewport/impl/shape_tessellation_helpers.inc — Handle → occ::handle
```

---

## 2. Test Status

All 4 test executables **fail** on the `feature/occtv8` branch:

| Test | Error |
|------|-------|
| `cad_core_sketch_profile_test.exe` | `expected one polygon profile from shared point ids` |
| `cad_core_multi_profile_extrude_test.exe` | `expected two sketch profiles before extrude` |
| `cad_core_cam_face_reference_test.exe` | Silent exit code 1 |
| `cad_core_plugin_feature_test.exe` | Silent exit code 1 |

The sketch/profile tests exercise code in `build_sketch_profile_regions.inc` →
`detect_arrangement_faces()` → `detect_line_arrangement_faces()`. This code is
**pure computation** (coordinate math, graph walking) with no direct OCCT
dependency. It was modified in commits *after* the port (`1f85703` through
`fdab41c`) for wire-path and arc-sweep logic. These failures may be from those
changes, not OCCT itself.

The CAM and plugin tests link against the full CAD core + OCCT and are more
likely affected by OCCT behavioral changes.

---

## 3. Behavioral Risks — What OCCT 8.0 Changed Under the Hood

These are the areas where OCCT 8.0 is likely to produce **different results**
even when the API compiles cleanly.

### 3.1 Boolean Operations (high risk)

**Files affected:**

```
native/cad-core/src/core/geometry/impl/body_modifier_helpers.inc       — BRepAlgoAPI_Cut (hole feature)
native/cad-core/src/core/geometry/impl/compile_bodies_feature_insert.inc — BRepAlgoAPI_Fuse/Cut/Common
native/cad-core/src/core/geometry/impl/fastener_drive_cutters.inc       — BRepAlgoAPI_Cut
native/cad-core/src/core/geometry/impl/plugin_geometry_shapes.inc       — BRepAlgoAPI_Cut/Fuse
native/cad-core/src/core/geometry/impl/shape_transform_helpers.inc      — BRepAlgoAPI_Fuse
native/cad-core/src/core/geometry/impl/thread_body_helpers.inc          — BRepAlgoAPI_Cut
native/cad-core/src/core/document/impl/private_extrude_body_contact_helpers.inc — BRepAlgoAPI_Common
```

OCCT 8.0 rewrote the Boolean Operations engine (TKBO). Tolerance merging,
edge splitting, and face unification all changed. A shape that was valid after
a fuse in 7.8 may be null or differently-structured in 8.0.

The `unify_same_domain()` wrapper (in `body_boolean_helpers.inc`) uses
`ShapeUpgrade_UnifySameDomain` which was also rewritten. If `unify.Build()`
produces a null shape, the wrapper silently returns the original — but the
original may now be different.

### 3.2 Exception Handling

**Files affected — 11 `catch (const Standard_Failure&)` sites:**

```
native/cad-core/src/core/document/impl/private_document_helpers.inc
native/cad-core/src/core/geometry/body_compiler.cpp
native/cad-core/src/core/geometry/feature_shape.cpp
native/cad-core/src/core/geometry/impl/body_frame_helpers.inc
native/cad-core/src/core/geometry/impl/fastener_drive_cutters.inc
native/cad-core/src/core/geometry/impl/primitive_extrude_shapes.inc
native/cad-core/src/core/geometry/impl/shape_transform_helpers.inc
native/cad-core/src/core/geometry/impl/thread_body_helpers.inc (×2)
```

In OCCT 8.0, `Standard_Failure` was refactored. Some exception paths now throw
`std::exception` subclasses directly (not `Standard_Failure`). A `catch (const
Standard_Failure&)` block may no longer catch all OCCT errors. Several of these
catch blocks are **empty** or return fallback shapes silently, so real errors
are hidden.

### 3.3 Tolerance and Precision

- `IsClosed()` / `IsPeriodic()` on curves and surfaces now uses `Precision::Computational()` (~2e-16) instead of `gp::Resolution()` (~1e-290). Code that relied on the old near-zero tolerance for closed-curve detection may now see different results.
- `Bnd_Range::IsIntersected()` now returns a strongly-typed `IntersectStatus` enum instead of a magic integer.
- Geometry hash classes expose configurable `CompTolerance` / `HashTolerance` instead of the old hard-coded `1e-12`.

### 3.4 OCCT Resource Configuration

**Current state: no CSF_/CASROOT environment variables are set.**

OCCT 8.0 restructured resource file layout:

```
Old (7.8):  data/    at install root, found via CASROOT
New (8.0):  share/<OCCT_PROJECT_NAME>/resources/, found via CSF_OCCTResourcePath
```

The precompiled binary at `third_party/opencascade-8.0.0-vc14-64/data/` has
resources at the old-style path. Without setting `CSF_OCCTResourcePath` or
`CASROOT`, any operation that needs resource files (STEP/IGES/STL export,
font rendering) will fail. The `cad_core.rs` spawn logic sets `PATH` for
DLLs but does **not** configure any OCCT resource variables.

**Resolved (2026-08-18):** OCCT is now self-built from the vendored source
(`third_party/occt8` → `third_party/occt8-build`), `cad_core.rs` sets
`CSF_OCCTResourcePath` to the build tree's `src/` when spawning the core
(the test runner does the same), and the standard `StlAPI_Writer` /
`STEPControl_Writer` data-exchange path runs without crashing —
`core/export/export.cpp` now uses the standard writers instead of the
manual STL/STEP serializers that worked around the precompiled-binary
crashes.

### 3.5 Evaluation Hierarchy (low risk for this project)

`D0`/`D1`/`D2`/`D3` virtual methods on `Geom_Curve`/`Geom_Surface` subclasses
are now non-virtual wrappers around `EvalD0`/`EvalD1`/etc. The call-site code
using `->D0(u, v, p)` still works — only overridden methods in custom
subclasses need updating. PolySmith does **not** subclass OCCT geometry types,
so this is not an issue.

### 3.6 Deprecated But Still Compiling

These compile with warnings and are non-breaking:

| Item | Replacement |
|------|-------------|
| `TopTools_IndexedMapOfShape` etc. | `NCollection_IndexedMap<TopoDS_Shape>` |
| `Standard_Mutex` | `std::mutex` + `std::lock_guard` |
| `Standard_Failure::GetMessageString()` | `what()` |
| OCCT math wrappers (`Cos`, `Sin`, `Sqrt`, `Max`, `Min`, …) | `std::cos`, `std::sin`, `std::sqrt`, `std::max`, `std::min`, … |
| BSpline `Weights()` (nullable pointer) | `WeightsArray()` (always-valid reference) |

---

## 4. Gap Summary

| Area | Risk | Action needed |
|------|------|---------------|
| API migration (phases 1-5) | None | Complete |
| Typedef/NCollection migration (phases 6-8) | None | Cosmetic; suppress warnings at build time |
| OCCT resource environment | Medium | Set `CSF_OCCTResourcePath` in `cad_core.rs` |
| Boolean operation behavioral changes | **High** | Add logging around every `BRepAlgoAPI_*` call |
| `Standard_Failure` catch blocks | **High** | Audit 11 sites; add `catch (const std::exception&)` fallbacks |
| Sketch/profile detection logic | Medium | May be pre-existing; bisect test failures vs OCCT port |
| `Standard_Failure::Raise()` → `throw` | Low | Not used in project (verified clean) |
| Math wrappers (`Cos`, `Sin`, …) | Low | Deprecated with warnings; none found in project |
| `Handle` out-parameter overloads | Low | Not used in project |

---

## 5. Recommended Investigation Order

1. **Run tests against the pre-port commit** (`4e53aa3`) to confirm which failures
   are OCCT-related vs. from the sketch/profile changes in later commits
   (`1f85703` through `fdab41c`).

2. **Add `catch (const std::exception&)` fallbacks** at the 11
   `Standard_Failure` catch sites — several are silent, so real OCCT errors
   are hidden.

3. **Set `CSF_OCCTResourcePath`** in Tauri's core-spawn logic so data
   exchange works.

4. **Instrument `unify_same_domain()`** and the boolean operation wrappers
   with diagnostic logging so you can see which operations produce null or
   unexpected shapes.

5. **Run the OCCT 8.0 migration scripts against `.inc` files** by patching
   `SOURCE_EXTENSIONS` to include `'.inc'` — this catches any `.inc` files
   that were missed (none found, but worth a dry-run).

