# Display Units — Metric / Inch Toggle

> **Shipped.** Core always works in mm; UI converts at the presentation boundary.
> Toggle in AppHeader switches between metric (mm, 2 decimal places) and
> imperial (in, 3 decimal places). Conversion utilities live in
> `apps/desktop-ui/src/utils/units.ts`.

## Goal

The CAD core always works in millimeters. The UI translates all dimension
display and input based on a user setting. When set to "inch", dimensions
are displayed in inches and user input is accepted in inches, then
converted to mm before reaching the core. Round-trip is lossless because
the core never changes its unit.

## Architecture Decision

**Unit conversion lives in the TypeScript layer only.** The C++ core has
no concept of display units — `SketchDimension.value` is always mm,
`ParameterEntry.resolved_value` is always mm, `viewport_state` coordinates
are always mm. The UI is the sole translator.

This keeps the boundary clean:

- C++ core: single source of truth in mm
- IPC: carries mm values only
- React: converts mm ↔ inch at the presentation boundary

## Where Units Flow

| Surface | Current state | Change |
|---|---|---|
| `DocumentState.units` | Always `"mm"` | Becomes user setting: `"mm"` or `"in"` |
| Dimension display (viewport sprites) | Raw mm value | Convert to display unit before rendering |
| Dimension editor input | Raw mm value | Accept in display unit, convert to mm before IPC |
| Draft dimension session values | Raw mm | Accept in display unit, convert to mm |
| Parameters panel Value column | Raw mm | Convert to display unit |
| Parameters panel Expression input | Already formula — no change needed | — |
| Sketch constraint labels | Raw mm/rad | Convert to display unit |
| Grid spacing | Raw mm | Convert to display unit |

## Settings Infrastructure

**New file:** `apps/desktop-ui/src/state/settingsStore.ts`

```ts
// Zustand or Jotai store. Persisted to localStorage.
interface UserSettings {
  displayUnits: "mm" | "in";
}
```

Default: `"mm"`. Persisted across app restarts. A simple toggle in the
app header or a settings panel.

## Conversion Utilities

**New file:** `apps/desktop-ui/src/utils/units.ts`

```ts
const MM_PER_INCH = 25.4;

export function mmToDisplay(mm: number, units: "mm" | "in"): number {
  return units === "in" ? mm / MM_PER_INCH : mm;
}

export function displayToMm(value: number, units: "mm" | "in"): number {
  return units === "in" ? value * MM_PER_INCH : value;
}

export function formatDimension(valueMm: number, units: "mm" | "in"): string {
  const display = mmToDisplay(valueMm, units);
  return units === "in"
    ? display.toFixed(3)  // thousandths of an inch
    : display.toFixed(2); // hundredths of a mm
}

export function parseDimensionInput(
  input: string, units: "mm" | "in"
): number | null {
  const parsed = parseFloat(input);
  if (isNaN(parsed)) return null;
  return displayToMm(parsed, units);
}
```

## Integration Points (ViewportPanel.tsx)

Every spot that reads or writes a dimension value must go through the
conversion layer:

| Code path | Current | Change |
|---|---|---|
| Dimension sprite label text | `dim.value.toFixed(2)` | `formatDimension(dim.value, settings.displayUnits)` |
| Dimension editor initial value | `dim.value.toString()` | `mmToDisplay(dim.value, settings.displayUnits).toString()` |
| Dimension editor submit | `parseFloat(rawValue)` → IPC | `parseDimensionInput(rawValue, settings.displayUnits)` → IPC |
| Draft dimension field values | Raw mm | Convert display→mm on each keystroke preview, display←mm on render |
| Parameters panel Value cell | `param.resolved_value.toFixed(2)` | `formatDimension(param.resolved_value, settings.displayUnits)` |
| Angle dimensions | Always degrees in display | `radToDeg(radians)` for display, `degToRad(degrees)` for input |

## DocumentState.units Propagation

- `DocumentState.units` currently hardcoded to `"mm"` in C++ at document
  creation.
- Change: make it settable from TypeScript via a `set_document_units` IPC
  command, or keep it set once at document creation based on the current
  user preference.
- Simpler v1 approach: `DocumentState.units` reflects the user setting at
  document creation time and is persisted in `.polysmith` files. When the
  user changes the display unit toggle mid-session, all open documents
  update their display. The stored `units` field is informational.

## Settings UI

- Add a small gear icon or "mm / in" toggle in `AppHeader.tsx` (right
  side, near the Parameters `f(x)` button).
- On click, toggle between `"mm"` and `"in"`.
- Re-renders all dimension displays immediately (React state → prop
  drilling or store subscription).

## Files Changed

| File | Change |
|---|---|
| `apps/desktop-ui/src/utils/units.ts` | **NEW** — conversion utilities |
| `apps/desktop-ui/src/state/settingsStore.ts` | **NEW** — user settings store |
| `apps/desktop-ui/src/layout/ViewportPanel.tsx` | Wire all dimension display/edit through `formatDimension` / `parseDimensionInput` |
| `apps/desktop-ui/src/layout/ParametersPanel.tsx` | Wire Value column through conversion |
| `apps/desktop-ui/src/layout/header/AppHeader.tsx` | Add mm/in toggle button |
| `apps/desktop-ui/src/i18n/en.json` | Add `settings.units` strings |
| `IPC-Protocol` | Document `set_document_units` if the IPC approach is taken |
