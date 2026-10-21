# Design System Specification: PolySmith Dual Theme System

## 1. Overview & Creative North Star
PolySmith supports first-class application themes:

- **Dark / Midnight Carbon:** the original high-contrast "Precision Nebula" workspace.
- **Light / Daylight Drafting:** a bright engineering-studio workspace that keeps the same cyan and yellow accent language while shifting the shell, panels, and viewport chrome to light neutral surfaces.
- **System:** a selector option that follows the operating system preference and resolves only to the built-in Dark or Light theme.
- **Catppuccin Latte, Frappé, Macchiato, and Mocha:** Catppuccin-derived themes using the official Catppuccin flavor palettes.
- **SolidCraft:** a neutral-slate theme with a muted blue accent, derived from the SolidCraft CAD application's dark palette. Colours only — no layout, spacing or component changes.

Both themes must feel like the same product. Theme work should change tokens, not component structure.

The experience is defined by **Atmospheric Depth**. By utilizing restrained neutral foundations and vibrant electric accents, we create a focused environment where the work (the 3D model) remains the primary object. We break the "template" look by avoiding rigid, opaque sidebars. Instead, we use floating translucent panels and asymmetric layouts that allow the workspace to bleed to the edges of the screen.

Theme tokens live in:

- `apps/desktop-ui/src/config/themes/dark.json`
- `apps/desktop-ui/src/config/themes/light.json`
- `apps/desktop-ui/src/config/themes/catppuccin-latte.json`
- `apps/desktop-ui/src/config/themes/catppuccin-frappe.json`
- `apps/desktop-ui/src/config/themes/catppuccin-macchiato.json`
- `apps/desktop-ui/src/config/themes/catppuccin-mocha.json`
- `apps/desktop-ui/src/config/themes/solidcraft.json`

The bundled `apps/desktop-ui/src/config/config.json` and bundled theme JSON files are defaults only. On app boot, PolySmith creates a user configuration directory when it does not already exist, copies the default `config.json` and theme files there, then reads and writes runtime settings from that user-owned location:

- macOS / Linux: `~/.config/polysmith/config.json` and `~/.config/polysmith/themes/`
- Windows: `%APPDATA%\polysmith\config.json` and `%APPDATA%\polysmith\themes\`

The active theme is selected by the user config file and the runtime settings UI. `system` is a selector value, not a standalone theme file; it checks `prefers-color-scheme` and resolves to `dark` or `light`. It must not resolve to Catppuccin themes.

Catppuccin attribution: the Catppuccin themes are based on the official
Catppuccin palette and style guide (`https://catppuccin.com/palette/` and
`https://github.com/catppuccin/catppuccin/blob/main/docs/style-guide.md`).
Keep the `Catppuccin` prefix in theme names and visible UI labels.

SolidCraft attribution: the `SolidCraft` theme derives its colour values from
the SolidCraft CAD application's dark palette
(`https://github.com/tangency-labs/solidcraft`, `assets/styles/ui/palettes.rs`,
MIT OR Apache-2.0) — colour values only; no code, artwork, icons or text is
taken, and PolySmith is not affiliated with or endorsed by SolidCraft. The name
is used descriptively so the theme is recognisable.

**Updating a bundled theme:** the copies under `%APPDATA%\polysmith\themes\`
(see below) win at runtime. Changing a bundled theme JSON only affects users who
have not run that build yet — the bootstrap writes a theme file if it is missing
and otherwise only backfills *missing* keys, so existing values stay. When a
bundled theme's values change, bump a `revision` field and have
`bootstrap_app_config` overwrite the stored copy when the bundled revision is
newer; until that exists, delete the stored file (or edit it) to pick the change
up.

---

### 2. Colors & Surface Philosophy
The palette is token-first. Components should consume CSS variables and never hardcode color values unless the value is an intentional fallback next to a theme token lookup.

#### Dark Theme: Midnight Carbon
The dark palette is built on a "Carbon-First" logic. We are not just using "Dark Mode"; we are creating a tonal landscape that minimizes eye fatigue during long engineering sessions.

*   **Primary Foundation:** `surface` (#131313) is our void. 
*   **The "No-Line" Rule:** Sectioning must be achieved through tonal shifts, not 1px solid lines. To separate a property panel from the viewport, use `surface-container-low` (#1c1b1b) against the `surface` background.
*   **Surface Hierarchy & Nesting:**
    *   **Level 0 (Workspace):** `surface-container-lowest` (#0e0e0e).
    *   **Level 1 (Main UI Shell):** `surface` (#131313).
    *   **Level 2 (Floating Panels):** `surface-container` (#201f1f) with 80% opacity and 12px backdrop blur.
    *   **Level 3 (Active Modals/Popovers):** `surface-container-highest` (#353534).
*   **The "Glass & Gradient" Rule:** Toolbars should not be flat. Apply a subtle linear gradient: `primary-container` (#00e5ff) at 5% opacity to `surface-variant` (#353534) at 20% opacity.
*   **Signature Textures:** For primary actions, use a "Glow-State" gradient from `primary` (#c3f5ff) to `primary-fixed-dim` (#00daf3).

#### Light Theme: Daylight Drafting
The light palette keeps the CAD-specific blue and yellow accents but moves the application shell into cool whites and pale blue-grays.

*   **Primary Foundation:** `surface` (#ffffff) and `surface-lowest` (#f4f7f8) create a clean drafting-studio base.
*   **Surface Hierarchy:**
    *   **Level 0 (Workspace):** `surface-lowest` (#f4f7f8).
    *   **Level 1 (Main UI Shell):** `surface` (#ffffff).
    *   **Level 2 (Floating Panels):** white panels at roughly 82-84% opacity, with pale blue-gray borders.
    *   **Level 3 (Active Modals/Popovers):** near-solid white with stronger neutral shadows.
*   **Accent Rule:** Cyan remains the primary action and active-state accent, but light mode uses deeper cyan text/edges (`#005f73` / `#00a8c8`) for contrast. Yellow construction-plane accents stay warm and visible, but should be less luminous than in dark mode.
*   **Depth Rule:** Light mode uses real soft shadows plus translucent borders. Glows should be lower intensity than dark mode and should never wash out text.
*   **Viewport Rule:** Light mode viewport backgrounds are cool off-whites. CAD bodies remain neutral gray, selected edges/vertices keep the warm orange highlight, and grid colors shift to blue-gray lines.

#### Catppuccin Themes
Catppuccin themes should preserve the source flavor semantics:

*   **Latte:** Light flavor. Use `base`, `mantle`, and `crust` for app backgrounds, `surface0-2` for raised panels, and `text/subtext*` for typography.
*   **Frappé / Macchiato / Mocha:** Dark flavors. Use `base`, `mantle`, and `crust` for shell and viewport depths, `surface0-2` for panel hierarchy, and `overlay0-2` for borders and muted marks.
*   **Catppuccin Accent Semantics:** Do not preserve PolySmith's default blue/yellow accent split in Catppuccin themes. Map PolySmith tokens to Catppuccin roles: `lavender` for active borders and primary active UI, `rosewater` for cursors and warm focus highlights, `blue` for link/tag-like semantics, `green` for success, `red` for errors, and `yellow` only for warnings.
*   **CAD Highlights:** Construction-plane and selected-geometry affordances should use Catppuccin highlight colors such as `peach`, `rosewater`, and `lavender`, while grid lines and neutral CAD structure should use `surface*` and `overlay*` colors rather than accent colors.
*   **Legibility First:** Follow the Catppuccin style guide's advice that text colors are guidelines and legibility can require deviations. For example, use `base` or `crust` as on-accent text when needed.
*   **No Raw Palette Use in Components:** Catppuccin hex values belong in the theme JSON files. Components consume PolySmith CSS variables only.

---

### 3. Typography: Technical Elegance
We pair **Space Grotesk** (Display/Headlines) with **Inter** (UI/Body) to balance "Future-Forward" branding with "Technical-Readout" legibility.

*   **Display (Space Grotesk):** Large, airy, and slightly letter-spaced (0.05em). Used for workspace titles and major mode indicators (e.g., *SKETCH MODE*).
*   **Headline & Title (Space Grotesk):** Used for panel headers. Use `headline-sm` (1.5rem) for main property groups.
*   **Body & Labels (Inter):** The workhorse. `label-md` (0.75rem) is the standard for parametric values. It must be high-contrast (`on-surface-variant`) to ensure readability against dark glass backgrounds.
*   **The "Digital Readout" Look:** All numeric inputs should use `label-md` with tabular lining figures to ensure numbers align perfectly in vertical lists.

---

### 4. Elevation & Depth: The Layering Principle
Dark mode rejects heavy traditional shadows. Light mode uses restrained shadows because they are legible on light surfaces. Both modes use **Luminance and Blur** to define depth.

*   **Tonal Layering:** Instead of a drop shadow, a floating "Glass" panel uses `outline-variant` (#3b494c) at 15% opacity as a "Ghost Border" to catch the light.
*   **Ambient Glows:** When a component is active (e.g., a selected edge in 3D space), it emits a `primary` (#c3f5ff) glow. Use a 12px blur at 20% opacity.
*   **Glassmorphism:** All side panels must use `backdrop-filter: blur(16px)`. This prevents the UI from feeling like a heavy "wall" and keeps the user connected to their 3D geometry behind the interface.
*   **Light Mode Shadows:** Use cool gray-blue shadows (`rgba(42, 61, 70, ...)`) from the light theme tokens. Avoid black-heavy shadows that make the UI look dirty.

---

### 5. Components

#### **Buttons & Active States**
*   **Primary Action:** Background: `primary-container` (#00e5ff). Text: `on-primary-fixed` (#001f24). Corner Radius: `md` (0.375rem).
*   **Ghost Toggle:** Background: transparent. Border: `outline-variant` (#3b494c) at 20%. On hover, background shifts to `surface-bright` (#393939).

#### **Parametric History Timeline**
*   **The Track:** A 2px line of `surface-container-highest`.
*   **Nodes:** 8px circles. Inactive: `outline`. Active: `primary` with a 4px `primary_container` outer glow. 
*   **Interaction:** On hover, nodes scale by 1.2x and reveal a floating glass tooltip.

#### **Floating Toolbars**
*   **Structure:** Horizontal clusters of icons. No containers around individual icons.
*   **Separation:** Use a 16px vertical gap between icon groups instead of a divider line.
*   **Active Tool:** A "pill" background using `secondary-container` (#11505a) with a `sm` (0.125rem) radius.

#### **Input Fields**
*   **Style:** Underline only. Use `outline-variant` for the default state. 
*   **Focus State:** The underline transforms into a 2px `primary_fixed` line with a soft glow. 
*   **Cards & Lists:** Strictly forbid divider lines. Use `surface-container-low` for the list background and `surface-container-high` for a hovered item to create separation.

---

### 6. Do’s and Don’ts

**Do:**
*   **Use Asymmetry:** Place the parametric history timeline floating at the bottom, offset from the side panels, to create a bespoke "engineered" feel.
*   **Embrace Transparency:** Ensure that the 3D grid is visible through the UI panels to maintain a sense of scale.
*   **Prioritize the "Primary" Accent:** Use the Electric Cyan (#00E5FF) sparingly. If everything glows, nothing is important.

**Don’t:**
*   **Don't use 100% White:** Use `on-surface` (#e5e2e1) for text. Pure white (#FFFFFF) is too jarring against the "Midnight Carbon" background and causes "halation" (visual bleeding).
*   **Don't use Solid Borders:** Avoid the "Boxy" look. If you need to define a boundary, use a 5% opacity shift or a 10% opacity "Ghost Border."
*   **Don't use Standard Easing:** All UI transitions (panel slides, node glows) should use a custom `cubic-bezier(0.16, 1, 0.3, 1)` for a "high-performance" feel.
*   **Don't Fork Components for Themes:** Add or adjust tokens in the theme JSON files instead. Components should remain theme-agnostic.
