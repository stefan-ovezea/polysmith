/**
 * Unit conversion utilities.
 *
 * The CAD core always works in millimeters. This is the sole translation
 * layer that the UI uses to display values in the user's preferred unit
 * and to convert user input back to mm before sending IPC commands.
 */

export type DisplayUnits = "mm" | "in";

const MM_PER_INCH = 25.4;

/** Convert a core mm value to the display unit. */
export function mmToDisplay(mm: number, units: DisplayUnits): number {
  return units === "in" ? mm / MM_PER_INCH : mm;
}

/** Convert a display-unit value back to mm for the core. */
export function displayToMm(value: number, units: DisplayUnits): number {
  return units === "in" ? value * MM_PER_INCH : value;
}

/**
 * Format a core mm value as a short, human-friendly string in the
 * active display unit. Follows the same trailing-zero cleanup that
 * the C++ side uses.
 */
function formatDimension(
  valueMm: number,
  units: DisplayUnits,
  decimals?: number,
): string {
  const display = mmToDisplay(valueMm, units);
  const prec = decimals ?? (units === "in" ? 3 : 2);
  const text = display.toFixed(prec);
  // Strip trailing zeros after decimal point (but keep at least one digit)
  const dot = text.indexOf(".");
  if (dot !== -1) {
    let trimmed = text.replace(/0+$/, "");
    if (trimmed.endsWith(".")) {
      trimmed = trimmed.slice(0, -1);
    }
    return trimmed;
  }
  return text;
}

/**
 * Parse a user-typed dimension input string into a core mm value.
 * Returns null if the string is not a valid number.
 */
export function parseDimensionInput(
  input: string,
  units: DisplayUnits,
): number | null {
  // Allow both "." and "," as decimal separator for international users
  const normalized = input.replace(",", ".");
  const parsed = parseFloat(normalized);
  if (isNaN(parsed)) return null;
  return displayToMm(parsed, units);
}

/**
 * Reformat a C++-generated dimension label (e.g. "12.35 mm", "D 24.70 mm",
 * "45\u00b0") into the target display unit. Angle labels are returned
 * unchanged since angles are always displayed in degrees.
 */
export function reformatDimensionLabel(
  label: string,
  kind: string,
  targetUnits: DisplayUnits,
): string {
  // Angles are always degrees — no unit conversion
  if (kind === "angle" || kind === "line_angle") {
    return label;
  }
  // Already in mm and target is mm — no change
  if (targetUnits === "mm") {
    return label;
  }

  // Strip the " mm" or other unit suffix, parse the number, convert
  const match = label.match(/^([A-Za-z]?\s*)([\d.]+)(.*)$/);
  if (!match) return label;

  const prefix = match[1]; // e.g. "D " or "R " or ""
  const rawValue = parseFloat(match[2]);
  const suffix = match[3]; // e.g. " mm" or ""

  if (isNaN(rawValue)) return label;

  const formatted = formatDimension(rawValue, "in", 3);
  const unitLabel = suffix.trim() === "mm" ? "in" : suffix.trim();
  return `${prefix}${formatted} ${unitLabel}`;
}
