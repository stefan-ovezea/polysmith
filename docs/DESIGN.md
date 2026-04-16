# Design System Specification: Midnight Carbon Aesthetic

## 1. Overview & Creative North Star: "The Precision Nebula"
This design system moves away from the clunky, mechanical interfaces of legacy CAD software and toward a "Precision Nebula" aesthetic. The goal is to make the user feel like they are operating a high-end spacecraft or a futuristic fabrication laboratory.

The experience is defined by **Atmospheric Depth**. By utilizing deep charcoal foundations and vibrant electric accents, we create a high-contrast environment where the work (the 3D model) is the brightest object in the room. We break the "template" look by avoiding rigid, opaque sidebars. Instead, we use floating, translucent panels and asymmetric layouts that allow the workspace to bleed to the very edges of the screen, creating an immersive, infinite canvas.

---

### 2. Colors & Surface Philosophy
The palette is built on a "Carbon-First" logic. We are not just using "Dark Mode"; we are creating a tonal landscape that minimizes eye fatigue during long engineering sessions.

*   **Primary Foundation:** `surface` (#131313) is our void. 
*   **The "No-Line" Rule:** Sectioning must be achieved through tonal shifts, not 1px solid lines. To separate a property panel from the viewport, use `surface-container-low` (#1c1b1b) against the `surface` background.
*   **Surface Hierarchy & Nesting:**
    *   **Level 0 (Workspace):** `surface-container-lowest` (#0e0e0e).
    *   **Level 1 (Main UI Shell):** `surface` (#131313).
    *   **Level 2 (Floating Panels):** `surface-container` (#201f1f) with 80% opacity and 12px backdrop blur.
    *   **Level 3 (Active Modals/Popovers):** `surface-container-highest` (#353534).
*   **The "Glass & Gradient" Rule:** Toolbars should not be flat. Apply a subtle linear gradient: `primary-container` (#00e5ff) at 5% opacity to `surface-variant` (#353534) at 20% opacity.
*   **Signature Textures:** For primary actions, use a "Glow-State" gradient from `primary` (#c3f5ff) to `primary-fixed-dim` (#00daf3).

---

### 3. Typography: Technical Elegance
We pair **Space Grotesk** (Display/Headlines) with **Inter** (UI/Body) to balance "Future-Forward" branding with "Technical-Readout" legibility.

*   **Display (Space Grotesk):** Large, airy, and slightly letter-spaced (0.05em). Used for workspace titles and major mode indicators (e.g., *SKETCH MODE*).
*   **Headline & Title (Space Grotesk):** Used for panel headers. Use `headline-sm` (1.5rem) for main property groups.
*   **Body & Labels (Inter):** The workhorse. `label-md` (0.75rem) is the standard for parametric values. It must be high-contrast (`on-surface-variant`) to ensure readability against dark glass backgrounds.
*   **The "Digital Readout" Look:** All numeric inputs should use `label-md` with tabular lining figures to ensure numbers align perfectly in vertical lists.

---

### 4. Elevation & Depth: The Layering Principle
We reject traditional shadows. In a dark CAD environment, shadows are often invisible. We use **Luminance and Blur** to define depth.

*   **Tonal Layering:** Instead of a drop shadow, a floating "Glass" panel uses `outline-variant` (#3b494c) at 15% opacity as a "Ghost Border" to catch the light.
*   **Ambient Glows:** When a component is active (e.g., a selected edge in 3D space), it emits a `primary` (#c3f5ff) glow. Use a 12px blur at 20% opacity.
*   **Glassmorphism:** All side panels must use `backdrop-filter: blur(16px)`. This prevents the UI from feeling like a heavy "wall" and keeps the user connected to their 3D geometry behind the interface.

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