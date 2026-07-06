import { ConstraintType, SketchTool } from "@/types";

const SelectIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M6 4.5v14l4.2-3 2.2 4.2 2.1-1.1-2.2-4.2 4.7-.7L6 4.5Z" />
  </svg>
);

const LineIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <circle cx="6" cy="18" r="1.6" />
    <circle cx="18" cy="6" r="1.6" />
    <path d="M7.4 16.6 16.6 7.4" />
  </svg>
);

export const DimensionIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.7"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M5 6v12" />
    <path d="M19 6v12" />
    <path d="M6.5 12h11" />
    <path d="m8.5 9.8-2.2 2.2 2.2 2.2" />
    <path d="m15.5 9.8 2.2 2.2-2.2 2.2" />
  </svg>
);

export const RectangleIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <rect x="5" y="7" width="14" height="10" rx="1.5" />
  </svg>
);

export const CircleIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <circle cx="12" cy="12" r="6.5" />
  </svg>
);

export const PolygonIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M12 5.5 18.5 10.2 16 18.5H8L5.5 10.2Z" />
  </svg>
);

export const ArcIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M6 15a6 6 0 1 1 12 0" />
    <circle cx="6" cy="15" r="1.4" />
    <circle cx="18" cy="15" r="1.4" />
  </svg>
);

const TrimIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="m7 17 10-10" />
    <path d="m9 7 8 8" />
  </svg>
);

const HorizontalConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M5 12h14" />
    <path d="m8 9-3 3 3 3" />
    <path d="m16 9 3 3-3 3" />
  </svg>
);

const VerticalConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M12 5v14" />
    <path d="m9 8 3-3 3 3" />
    <path d="m9 16 3 3 3-3" />
  </svg>
);

const PerpendicularConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M7 6v12" />
    <path d="M7 12h10" />
  </svg>
);

const CoincidentConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <circle cx="8.5" cy="12" r="3.2" />
    <circle cx="15.5" cy="12" r="3.2" />
  </svg>
);

const ParallelConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M7 8h10" />
    <path d="M7 16h10" />
    <path d="m9 6-2 2 2 2" />
    <path d="m15 14 2 2-2 2" />
  </svg>
);

const EqualLengthConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M6 9h12" />
    <path d="M6 15h12" />
  </svg>
);

const MirrorConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    {/* Vertical dashed mirror axis flanked by two arrow-heads
        pointing toward each other — visually conveys "reflect
        across this line". */}
    <path d="M12 4v16" strokeDasharray="2 2" />
    <path d="M4 9l4 3-4 3" />
    <path d="M20 9l-4 3 4 3" />
  </svg>
);

const ClearConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="m7 7 10 10" />
    <path d="m17 7-10 10" />
  </svg>
);

export function SketchToolIcon({ tool }: { tool: SketchTool }) {
  switch (tool) {
    case "select":
      return <SelectIcon />;
    case "line":
      return <LineIcon />;
    case "dimension":
      return <DimensionIcon />;
    case "rectangle":
      return <RectangleIcon />;
    case "circle":
      return <CircleIcon />;
    case "polygon":
      return <PolygonIcon />;
    case "arc":
      return <ArcIcon />;
    case "fillet":
      return <FilletIcon />;
    case "project":
      return <ProjectFaceIcon />;
    default:
      return <TrimIcon />;
  }
}

// 3D primitive / modeling icons. Drawn as flat-stroke SVGs in the same
// 24×24 grid as the sketch icons so they line up inside the shared
// `cad-icon-button` chrome. Isometric projection (cos30 ≈ 0.866,
// sin30 = 0.5) so the cube/cylinder/etc. read as 3D in the toolbar.

export const BoxIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M12 4 4 8v8l8 4 8-4V8Z" />
    <path d="m4 8 8 4 8-4" />
    <path d="M12 12v8" />
  </svg>
);

export const CylinderIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <ellipse cx="12" cy="6" rx="6" ry="2.4" />
    <path d="M6 6v12" />
    <path d="M18 6v12" />
    <path d="M6 18a6 2.4 0 0 0 12 0" />
  </svg>
);

const SphereIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <circle cx="12" cy="12" r="7.5" />
    <ellipse cx="12" cy="12" rx="7.5" ry="3" />
  </svg>
);

// contextual modeling Extrude icon: dominant isometric cube taking most of
// the viewbox, with a small vertical arrow tucked into the upper-right
// corner. The size ratio (cube ~12 units wide vs. arrow ~4 units) is
// what reads "push the profile *up* to make this body" rather than
// "two equal symbols side by side".
export const ExtrudeIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    {/* Cube: front silhouette + top face + center vertical edge,
        spanning x=2..14, y=5..21. */}
    <path d="M8 5 2 8v10l6 3 6-3V8Z" />
    <path d="m2 8 6 3 6-3" />
    <path d="M8 11v10" />
    {/* Small upward arrow on the right, half the cube's height. */}
    <path d="M19 17v-7" />
    <path d="m17 12 2-2 2 2" />
  </svg>
);

export const LoftIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <rect x="4" y="14" width="8" height="6" rx="1" />
    <circle cx="17" cy="7" r="3" />
    <path d="m12 14 5-4" />
    <path d="m4 14 11-7" />
  </svg>
);

export const RevolveIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M7 18V6" />
    <path d="M11 8.5c4.4.7 7 2.2 7 3.5s-2.6 2.8-7 3.5" />
    <path d="M11 6v12" />
    <path d="M11 6c2.4 1.4 3.8 3.4 3.8 6S13.4 16.6 11 18" />
    <path d="m17 9 2 3-2 3" />
  </svg>
);

export const HoleIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M5 8.5 12 5l7 3.5v7L12 19l-7-3.5Z" />
    <ellipse cx="12" cy="12" rx="3.2" ry="1.7" />
    <path d="M8.8 12v2.2c0 .9 1.4 1.7 3.2 1.7s3.2-.8 3.2-1.7V12" />
  </svg>
);

export const ThreadIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M8 5.5c0-1.1 1.8-2 4-2s4 .9 4 2v13c0 1.1-1.8 2-4 2s-4-.9-4-2Z" />
    <path d="M8 5.5c0 1.1 1.8 2 4 2s4-.9 4-2" />
    <path d="M8 9c1.4 1 6.6 1 8 0" />
    <path d="M8 12c1.4 1 6.6 1 8 0" />
    <path d="M8 15c1.4 1 6.6 1 8 0" />
    <path d="M8 18c1.4 1 6.6 1 8 0" />
  </svg>
);

export const FastenerIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M8 5.5 12 3.5l4 2v3L12 10.5l-4-2Z" />
    <path d="M9.5 9.7v9.1c0 .9 1.1 1.7 2.5 1.7s2.5-.8 2.5-1.7V9.7" />
    <path d="M9.5 12.5c.9.6 4.1.6 5 0" />
    <path d="M9.5 15c.9.6 4.1.6 5 0" />
    <path d="M9.5 17.5c.9.6 4.1.6 5 0" />
  </svg>
);

const PatternIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <rect x="4" y="4" width="6" height="6" rx="1" />
    <rect x="14" y="4" width="6" height="6" rx="1" />
    <rect x="4" y="14" width="6" height="6" rx="1" />
    <rect x="14" y="14" width="6" height="6" rx="1" />
  </svg>
);

const SketchIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M4 20 6 14 16 4l4 4L10 18Z" />
    <path d="m14 6 4 4" />
  </svg>
);

// Project glyph: source outline, projection rays, and target sketch
// outline. Kept deliberately simple so it reads at toolbar size.
const ProjectFaceIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <rect x="5" y="5" width="7" height="7" rx="1" />
    <rect x="12" y="12" width="7" height="7" rx="1" />
    <path d="M12 8.5h3.5v3.5" />
    <path d="M12 12 7.5 16.5" strokeDasharray="1.5 2" />
    <path d="M15.5 8.5 8.5 15.5" />
  </svg>
);

export const FilletIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M5 19V12a7 7 0 0 1 7-7h7" />
    <path d="M5 19h14" />
  </svg>
);

export const ChamferIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M5 19V10l5-5h9" />
    <path d="M5 19h14" />
  </svg>
);

// Toolbar-sized parallelogram + offset arrow. Reads as "duplicate
// this plane out along its normal at a typed distance". Uses the
// same h-7 w-7 grid as the other ribbon glyphs so it lines up with
// Box / Cylinder / Extrude in the Construct ribbon.
export const OffsetPlaneIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M4 8h11l-2 8H2Z" />
    <path d="M11 4h11l-2 8H9Z" />
    <path d="m17 6 2-2-2-2" />
  </svg>
);

// Two parallel planes with a midplane between them. Disabled
// placeholder for the Construct ribbon's "Midplane" tool.
export const MidplaneIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M3 6h11l-1 4H2Z" />
    <path d="M3 18h11l-1-4H2Z" />
    <path d="M5 12h12" strokeDasharray="2 2" />
  </svg>
);

export const TangentPlaneIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M4 16c2.4-5.8 8.6-8 16-6" />
    <path d="M5 11h14l-2 7H3Z" />
    <path d="M13 5v4" />
  </svg>
);

export const AnglePlaneIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M4 17h11l-2-7H2Z" />
    <path d="M8 16 18 6" />
    <path d="M12.5 11.5a4 4 0 0 1 3 3" />
    <path d="m15.5 14.5 1.2-1.8" />
  </svg>
);

// Construction axis: thin line with arrow tips. Distinguishable from
// the origin XYZ trihedron at this scale.
export const ConstructAxisIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M4 12h16" />
    <path d="m6 10-2 2 2 2" />
    <path d="m18 10 2 2-2 2" />
  </svg>
);

// Single dot for the Construct ribbon's "Point" placeholder. Drawn
// as a filled circle so it reads as a point rather than a hole.
export const ConstructPointIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="currentColor"
    stroke="currentColor"
    strokeWidth="1.4"
    aria-hidden="true"
  >
    <circle cx="12" cy="12" r="2.6" />
  </svg>
);

export const HelixIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M12 4v16" />
    <path d="M7 6c0 1.4 2.2 2.5 5 2.5s5 1.1 5 2.5-2.2 2.5-5 2.5-5 1.1-5 2.5 2.2 2.5 5 2.5" />
  </svg>
);

// Face with bidirectional arrows. Reads "push or pull this face".
export const PressPullIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <rect x="4" y="6" width="9" height="12" rx="1" />
    <path d="M16 12h6" />
    <path d="m20 10 2 2-2 2" />
    <path d="m18 10-2 2 2 2" />
  </svg>
);

// Hollowed cube — "shell out the inside of this body".
export const ShellIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M12 4 4 8v8l8 4 8-4V8Z" />
    <path d="m4 8 8 4 8-4" />
    <path d="M12 12v8" />
    <path d="M9 9.5v5L12 16l3-1.5v-5" strokeDasharray="2 2" />
  </svg>
);

// Profile swept along a path — a face moving along a guide curve.
export const SweepIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M5 16c4-8 10-8 14-3" />
    <path d="M6 7h5l2 4-3 4H5l-2-4Z" />
    <path d="m16 12 3 1 1-3" />
  </svg>
);

// Four-direction move gizmo. Disabled placeholder for the Modify
// ribbon's "Move" tool.
export const MoveIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <path d="M12 4v16" />
    <path d="M4 12h16" />
    <path d="m9 7 3-3 3 3" />
    <path d="m9 17 3 3 3-3" />
    <path d="m7 9-3 3 3 3" />
    <path d="m17 9 3 3-3 3" />
  </svg>
);

// Generic fallback used by the feature timeline for the synthetic
// "root" entry and any future feature kinds we haven't drawn yet.
const FeatureGenericIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-7 w-7"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
    aria-hidden="true"
  >
    <circle cx="12" cy="12" r="3" />
  </svg>
);

// Small parallelogram icon shared by every parametric construction
// plane in the timeline / hierarchy. Visually distinct from the
// origin axes badge and the body / sketch glyphs.
const ConstructionPlaneIcon = () => (
  <svg
    viewBox="0 0 24 24"
    width="16"
    height="16"
    aria-hidden="true"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.6"
    strokeLinecap="round"
    strokeLinejoin="round"
  >
    <path d="M5 8h12l2 8H7Z" />
    <path d="M5 8L7 16" />
    <path d="M17 8l2 8" />
  </svg>
);

export function FeatureKindIcon({ kind }: { kind: string }) {
  switch (kind) {
    case "box":
      return <BoxIcon />;
    case "cylinder":
      return <CylinderIcon />;
    case "extrude":
      return <ExtrudeIcon />;
    case "sphere":
      return <SphereIcon />;
    case "loft":
      return <LoftIcon />;
    case "revolve":
      return <RevolveIcon />;
    case "sweep":
      return <SweepIcon />;
    case "hole":
      return <HoleIcon />;
    case "thread":
      return <ThreadIcon />;
    case "fastener":
      return <FastenerIcon />;
    case "helix":
      return <HelixIcon />;
    case "pattern":
      return <PatternIcon />;
    case "sketch":
      return <SketchIcon />;
    case "fillet":
      return <FilletIcon />;
    case "chamfer":
      return <ChamferIcon />;
    case "construction_plane":
      return <ConstructionPlaneIcon />;
    default:
      return <FeatureGenericIcon />;
  }
}

const FixConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    width="16"
    height="16"
    fill="none"
    stroke="currentColor"
    strokeWidth="1.8"
    strokeLinecap="round"
    strokeLinejoin="round"
  >
    <circle cx="12" cy="5" r="2" />
    <line x1="12" y1="7" x2="12" y2="20" />
    <line x1="8" y1="10" x2="16" y2="10" />
  </svg>
);

export function ConstraintIcon({ kind }: { kind: ConstraintType }) {
  switch (kind) {
    case "horizontal":
      return <HorizontalConstraintIcon />;
    case "vertical":
      return <VerticalConstraintIcon />;
    case "perpendicular":
      return <PerpendicularConstraintIcon />;
    case "coincident":
      return <CoincidentConstraintIcon />;
    case "parallel":
      return <ParallelConstraintIcon />;
    case "equal_length":
      return <EqualLengthConstraintIcon />;
    case "fixed":
      return <FixConstraintIcon />;
    case "mirror":
      return <MirrorConstraintIcon />;
    default:
      return <ClearConstraintIcon />;
  }
}
