import { ConstraintType, SketchTool } from "@/types";

export const SelectIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-4 w-4"
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

export const LineIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-4 w-4"
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

export const RectangleIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-4 w-4"
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
    className="h-4 w-4"
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

export const ArcIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-4 w-4"
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

export const TrimIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-4 w-4"
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

export const HorizontalConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-4 w-4"
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

export const VerticalConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-4 w-4"
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

export const PerpendicularConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-4 w-4"
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

export const CoincidentConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-4 w-4"
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

export const ParallelConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-4 w-4"
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

export const EqualLengthConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-4 w-4"
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

export const ClearConstraintIcon = () => (
  <svg
    viewBox="0 0 24 24"
    className="h-4 w-4"
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
    case "rectangle":
      return <RectangleIcon />;
    case "circle":
      return <CircleIcon />;
    case "arc":
      return <ArcIcon />;
    default:
      return <TrimIcon />;
  }
}

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
    default:
      return <ClearConstraintIcon />;
  }
}
