# Dimension Tool — Split Button & Floating Panel Implementation Plan

**Status:** Planning  
**Author:** CodeWhale (deepseek-v4-pro)  
**Date:** 2026-06-15

## Objective

Convert the Sketch **Dimension** toolbar button from a plain `<button>` to a `SplitToolButton` (matching the pattern used by Rectangle, Circle, Arc, Polygon). Add a floating panel that appears when the dimension tool is active. The dropdown lists industry-standard dimension types; only **Auto** is functional (it preserves the existing dimension tool behaviour). The other entries are reserved for future ISO drawing use.

## Context

Today the dimension button is a simple `<button>` rendered inside the `sketchTools.filter(t => t.id !== "rectangle" && t.id !== "arc" && t.id !== "circle" && t.id !== "polygon")` block in `SketchToolbar.tsx` (line 161). Tools that have split buttons (Rectangle, Arc, Circle, Polygon) are filtered out of that block and rendered separately as `<SplitToolButton>` components.

When the dimension tool is active (`activeSketchTool === "dimension"`), the viewport shows a `DimensionToolHint` overlay (a small instruction label at top-left). No floating panel is rendered because `isDrawableSketchTool("dimension")` returns `false`.

## Dimension Types (Dropdown)

The industry-standard list of dimension types:

| Value | Label | Status |
|---|---|---|
| `auto` | Auto Dimension | **Functional** — preserves existing behaviour |
| `linear` | Linear Dimension | Placeholder |
| `aligned` | Aligned Dimension | Placeholder |
| `angular` | Angular Dimension | Placeholder |
| `radius` | Radius Dimension | Placeholder |
| `diameter` | Diameter Dimension | Placeholder |
| `ordinate` | Ordinate Dimension | Placeholder |
| `jogged_radial` | Jogged Radial Dimension | Placeholder |
| `arc_length` | Arc Length Dimension | Placeholder |
| `curve_min_max` | Curve Min / Max Dimension | Placeholder |
| `baseline` | Baseline Dimension | Placeholder |
| `chain` | Chain Dimension | Placeholder |

Additional dimension-utility tools (for future drawing sheet context):

| Value | Label | Status |
|---|---|---|
| `tidy_up` | Tidy Up | Placeholder |
| `arrange` | Arrange Dimensions | Placeholder |
| `flip_arrows` | Flip Arrows | Placeholder |
| `match` | Match Dimension | Placeholder |
| `dimension_break` | Dimension Break | Placeholder |

The user explicitly stated: "I do not want to implement any functionality so far for any of the items in this list except the Auto mode." The dropdown entries beyond Auto can all do the same thing as Auto for now (or be disabled), but the dropdown must be visible so the user can study and plan around the full set.

## Files to Modify

### 1. Types — `apps/desktop-ui/src/types/geometry/sketch.ts`

Add a `DimensionToolMode` type near the existing tool-mode types (after line 449):

```ts
/** Dimension tool modes for the split tool button.
 *  Only "auto" is functional today; the rest are reserved for
 *  future ISO drawing-sheet dimensioning. */
export type DimensionToolMode =
  | "auto"
  | "linear"
  | "aligned"
  | "angular"
  | "radius"
  | "diameter"
  | "ordinate"
  | "jogged_radial"
  | "arc_length"
  | "curve_min_max"
  | "baseline"
  | "chain"
  | "tidy_up"
  | "arrange"
  | "flip_arrows"
  | "match"
  | "dimension_break";
```

Export it from the types barrel if needed (check `apps/desktop-ui/src/types/index.ts`).

### 2. i18n — `apps/desktop-ui/src/i18n/en.json`

Add translation keys under `toolbar` for each dimension type. Keep existing `toolbar.dimension` ("Dimension") as the primary label.

New keys to add in the `toolbar` block:

```json
"dimensionAuto": "Auto",
"dimensionLinear": "Linear",
"dimensionAligned": "Aligned",
"dimensionAngular": "Angular",
"dimensionRadius": "Radius",
"dimensionDiameter": "Diameter",
"dimensionOrdinate": "Ordinate",
"dimensionJoggedRadial": "Jogged Radial",
"dimensionArcLength": "Arc Length",
"dimensionCurveMinMax": "Curve Min / Max",
"dimensionBaseline": "Baseline",
"dimensionChain": "Chain",
"dimensionTidyUp": "Tidy Up",
"dimensionArrange": "Arrange",
"dimensionFlipArrows": "Flip Arrows",
"dimensionMatch": "Match",
"dimensionBreak": "Break",
```

Also add a `viewport.dimensionToolTitle` key:

```json
"dimensionToolTitle": "Dimension",
```

### 3. SketchToolbar — `apps/desktop-ui/src/layout/header/SketchToolbar.tsx`

**Props:**

Add to the `SketchToolbarProps` interface:

```ts
dimensionToolMode: DimensionToolMode;
onSetDimensionToolMode: (mode: DimensionToolMode) => void;
```

Import `DimensionToolMode` from `@/types`.

**Toolbar rendering:**

- Add `"dimension"` to the filter exclusion list on line 161 so the plain button is no longer rendered:
  ```
  .filter(t => t.id !== "rectangle" && t.id !== "arc" && t.id !== "circle" && t.id !== "polygon" && t.id !== "dimension")
  ```
- Add a new `<span>` block (after the polygon button, before the constraint controls separator) with a `SplitToolButton` for dimension, following the same pattern as Circle/Rectangle:

```tsx
<span
  onMouseEnter={(e) => openHelp("dimension", e.currentTarget)}
  onMouseLeave={closeHelp}
  onFocus={(e) => openHelp("dimension", e.currentTarget)}
  onBlur={closeHelp}
>
<SplitToolButton
  options={[
    { value: "auto" as const, label: t("toolbar.dimensionAuto") },
    { value: "linear" as const, label: t("toolbar.dimensionLinear") },
    { value: "aligned" as const, label: t("toolbar.dimensionAligned") },
    { value: "angular" as const, label: t("toolbar.dimensionAngular") },
    { value: "radius" as const, label: t("toolbar.dimensionRadius") },
    { value: "diameter" as const, label: t("toolbar.dimensionDiameter") },
    { value: "ordinate" as const, label: t("toolbar.dimensionOrdinate") },
    { value: "jogged_radial" as const, label: t("toolbar.dimensionJoggedRadial") },
    { value: "arc_length" as const, label: t("toolbar.dimensionArcLength") },
    { value: "curve_min_max" as const, label: t("toolbar.dimensionCurveMinMax") },
    { value: "baseline" as const, label: t("toolbar.dimensionBaseline") },
    { value: "chain" as const, label: t("toolbar.dimensionChain") },
    { value: "tidy_up" as const, label: t("toolbar.dimensionTidyUp") },
    { value: "arrange" as const, label: t("toolbar.dimensionArrange") },
    { value: "flip_arrows" as const, label: t("toolbar.dimensionFlipArrows") },
    { value: "match" as const, label: t("toolbar.dimensionMatch") },
    { value: "dimension_break" as const, label: t("toolbar.dimensionBreak") },
  ]}
  value={dimensionToolMode}
  onChange={onSetDimensionToolMode}
  onPrimaryAction={() => {
    onCancelSketchConstraint();
    void onSetSketchTool("dimension");
  }}
  isActive={activeSketchPlaneId ? activeSketchTool === "dimension" : false}
  disabled={!activeSketchPlaneId}
  tooltip={toolLabel(
    sketchTools.find((t) => t.id === "dimension")!,
  )}
  ariaLabel={t("toolbar.dimension")}
>
  <DimensionIcon />
</SplitToolButton>
</span>
```

**Note on `DimensionIcon`:** Currently `DimensionIcon` is a private (unexported) component in `ToolBarIcons.tsx`. It needs to be exported so the SketchToolbar can use it directly (the `SketchToolIcon` wrapper won't work here since `SplitToolButton` expects `children` as an icon element, not a tool-id string). The existing pattern for Rectangle, Circle, Arc, Polygon already exports their icons directly — `DimensionIcon` should be exported the same way.

### 4. App.tsx — State management

**Add state:**

```ts
const [dimensionToolMode, setDimensionToolMode] = useState<DimensionToolMode>("auto");
```

**Add callback:**

```ts
const onSetDimensionToolMode = useCallback((mode: DimensionToolMode) => {
  setDimensionToolMode(mode);
}, []);
```

**Pass to SketchToolbar** (near line 1269):

```tsx
dimensionToolMode={dimensionToolMode}
onSetDimensionToolMode={onSetDimensionToolMode}
```

**Pass to ViewportPanelShell** (near line 1756):

```tsx
dimensionToolMode={dimensionToolMode}
onSetDimensionToolMode={onSetDimensionToolMode}
```

### 5. SketchToolPanel — `apps/desktop-ui/src/layout/viewport/SketchToolPanel.tsx`

Currently the component returns `null` when `!isDrawableSketchTool(activeSketchTool)`, which excludes dimension. We need to show a panel for dimension too.

**Approach:** Change the early return condition to also allow `"dimension"`:

```tsx
if (!isDrawableSketchTool(activeSketchTool) && activeSketchTool !== "dimension") {
  return null;
}
```

**Add dimension-specific content** below the title. After the existing polygon block (or inside a new conditional block):

```tsx
{activeSketchTool === "dimension" && (
  <div>
    <p className="cad-kicker">{translate("viewport.mode")}</p>
    <div className="mt-3">
      <Dropdown
        value={dimensionToolMode}
        options={[
          { value: "auto", label: translate("toolbar.dimensionAuto") },
          { value: "linear", label: translate("toolbar.dimensionLinear") },
          { value: "aligned", label: translate("toolbar.dimensionAligned") },
          { value: "angular", label: translate("toolbar.dimensionAngular") },
          { value: "radius", label: translate("toolbar.dimensionRadius") },
          { value: "diameter", label: translate("toolbar.dimensionDiameter") },
          { value: "ordinate", label: translate("toolbar.dimensionOrdinate") },
          { value: "jogged_radial", label: translate("toolbar.dimensionJoggedRadial") },
          { value: "arc_length", label: translate("toolbar.dimensionArcLength") },
          { value: "curve_min_max", label: translate("toolbar.dimensionCurveMinMax") },
          { value: "baseline", label: translate("toolbar.dimensionBaseline") },
          { value: "chain", label: translate("toolbar.dimensionChain") },
        ]}
        label={translate("viewport.mode")}
        onChange={(value) => {
          onSetDimensionToolMode(value as DimensionToolMode);
        }}
      />
    </div>
  </div>
)}
```

**Update props interface** to include:

```ts
dimensionToolMode: DimensionToolMode;
onSetDimensionToolMode: (mode: DimensionToolMode) => void;
```

**Update title/subtitle** — add a dimension case to the subtitle line:

```tsx
{activeSketchTool === "dimension" && (
  <p className="text-xs text-on-surface/50 mt-1">
    {translate(
      dimensionToolMode === "auto"
        ? "toolbar.dimensionAuto"
        : dimensionToolMode === "linear"
          ? "toolbar.dimensionLinear"
          // ... etc
    )}
  </p>
)}
```

Actually, for simplicity we can use the translation key directly:

```tsx
{activeSketchTool === "dimension" && (
  <p className="text-xs text-on-surface/50 mt-1">
    {translate(`toolbar.dimension${dimensionToolMode.charAt(0).toUpperCase() + dimensionToolMode.slice(1).replace(/_([a-z])/g, (_, c) => c.toUpperCase())}`)}
  </p>
)}
```

Or simpler: keep a lookup map or just use the key dynamically since the i18n keys follow a predictable pattern.

**Hide construction checkbox for dimension:** The construction checkbox is not meaningful for the dimension tool. Add a condition:

```tsx
{activeSketchTool !== "dimension" && (
  <label className="flex items-center justify-between gap-4 text-sm text-on-surface">
    {/* construction checkbox */}
  </label>
)}
```

### 6. ViewportPanelShell — `apps/desktop-ui/src/layout/viewport/ViewportPanelShell.tsx`

Add `dimensionToolMode` and `onSetDimensionToolMode` to the props interface and pass them through to `SketchToolPanel`.

### 7. ToolBarIcons — `apps/desktop-ui/src/layout/header/ToolBarIcons.tsx`

Export the `DimensionIcon` component. Currently (line 35) it's a `const` with no `export` keyword. Change:

```tsx
export const DimensionIcon = () => (
```

And update the `SketchToolIcon` function to use the exported version (it already references `DimensionIcon` locally which will now point to the exported one).

Also update the import in `SketchToolbar.tsx` line 6 to include `DimensionIcon`:

```tsx
import { SketchToolIcon, RectangleIcon, ArcIcon, CircleIcon, PolygonIcon, DimensionIcon } from "./ToolBarIcons";
```

### 8. Help — `help/help-index.ts`

The existing `dimensionEntry` is already defined (around line 224). No changes needed to the help content, but verify the entry is still reachable from the new `SplitToolButton` tooltip (it is, via `openHelp("dimension", ...)`).

### 9. CSS

Verify that the existing `.cad-split-tool-*` styles work correctly for the new dimension split button. The SplitToolButton component uses generic classes that are already styled. No new CSS should be needed unless the dropdown menu with 17 items needs scroll — the `cad-scrollbar` class on the menu already handles overflow.

## What NOT to change

- **Core dimension logic:** No changes to any dimension action files (`draftDimensionActions.ts`, `dimensionToolActions.ts`, `dimensionEditorActions.ts`, etc.). The dimension tool's behavior (click line → length, click circle → radius, click two entities → distance/angle) is unchanged.
- **IPC protocol:** No protocol changes. The dimension tool mode is purely UI state; the core does not need to know about it yet.
- **Hotkeys:** The `D` hotkey for dimension is unchanged. It arms the dimension tool (which defaults to Auto mode).
- **Sketch selection/click handling:** The `activeSketchTool === "dimension"` checks throughout the viewport code remain unchanged.

## Verification Plan

1. **Build check:** `cd apps/desktop-ui && npx tsc --noEmit` — TypeScript must compile with no errors.
2. **Toolbar visual:** The Dimension button now has a chevron (▼) and opens a dropdown menu with all 17 dimension types.
3. **Primary click:** Clicking the Dimension icon directly arms the dimension tool in Auto mode (same as today).
4. **Floating panel:** When dimension is active, a floating panel appears showing "Dimension" as title, the current mode as subtitle, and a mode dropdown.
5. **Mode switching:** Selecting a different mode from the dropdown changes the subtitle but does not alter the dimension tool's behaviour (all modes currently behave as Auto).
6. **Existing dimension functionality:** Creating dimensions (line length, circle radius, point distance, angle, etc.) works exactly as before.
7. **Hotkey:** Pressing `D` arms the dimension tool and shows the floating panel.
8. **Sketch exit:** Finishing the sketch or switching to another tool hides the dimension floating panel.
9. **i18n fallback:** All translation keys resolve (test by setting language to a locale that falls back to English).

## Risks & Unknowns

- **Dropdown length:** 17 items is long. The dropdown menu has `cad-scrollbar` styling so it will scroll. This is acceptable.
- **Future direction:** The user may want to split "dimension-creation" tools (Auto, Linear, Aligned, Angular, Radius, Diameter, etc.) from "dimension-utility" tools (Tidy Up, Arrange, Flip Arrows, Match, Break). For now they all live in one dropdown for discoverability.
- **Construction checkbox:** Not shown for dimension tool, but the `sketchToolConstruction` state is still managed in App.tsx. If the user had construction enabled for line tool and then switches to dimension, the checkbox disappears — this is correct UX since dimensions don't have construction mode.

## Files Summary

| File | Change |
|---|---|
| `apps/desktop-ui/src/types/geometry/sketch.ts` | Add `DimensionToolMode` type export |
| `apps/desktop-ui/src/i18n/en.json` | Add 17 dimension type translation keys |
| `apps/desktop-ui/src/layout/header/ToolBarIcons.tsx` | Export `DimensionIcon` |
| `apps/desktop-ui/src/layout/header/SketchToolbar.tsx` | Convert dimension to `SplitToolButton` |
| `apps/desktop-ui/src/App.tsx` | Add `dimensionToolMode` state + propagation |
| `apps/desktop-ui/src/layout/viewport/SketchToolPanel.tsx` | Add dimension floating panel content |
| `apps/desktop-ui/src/layout/viewport/ViewportPanelShell.tsx` | Pass dimension props through |
