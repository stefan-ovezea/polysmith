// Theme token access for the Three.js render path (D-002).
//
// `themeColor()` returns the CSS string verbatim, which is correct for CSS/SVG
// and canvas 2D consumers. A MATERIAL colour is a different contract: Three's
// `Color.set`/`new Color` silently DROPS the alpha channel of `rgba(...)` (and
// logs "THREE.Color: Alpha component ... will be ignored" once per string), so a
// token that carries alpha must be split at this boundary.
//
//   themeColor(token, fallback)        CSS / SVG / canvas 2D - alpha honoured
//   themeColorSolid(token, fallback)   material colour - alpha stripped
//   themeColorWithAlpha(token, fallback) both halves, for callers that want
//                                      to drive material.opacity from the token
//
// A material that wants translucency sets `transparent` and `opacity`
// explicitly; the token's alpha is not a substitute.

export interface ThemeColorParts {
  /** Alpha-free colour string (`#rrggbb` or `rgb(...)`). */
  color: string;
  /** Alpha from the token, 0..1; 1 when the token has none. */
  opacity: number;
}

function resolveToken(token: string, fallback: string): string {
  if (typeof document === "undefined") {
    return fallback;
  }
  const value = getComputedStyle(document.documentElement)
    .getPropertyValue(token)
    .trim();
  return value || fallback;
}

/** Split a CSS colour string into an alpha-free colour and its alpha. */
export function splitCssColorAlpha(value: string): ThemeColorParts {
  const rgba = value.match(
    /^rgba?\(\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*(?:,\s*([0-9.]+)\s*)?\)$/i,
  );
  if (rgba) {
    const red = Math.round(Number(rgba[1]));
    const green = Math.round(Number(rgba[2]));
    const blue = Math.round(Number(rgba[3]));
    const opacity = rgba[4] === undefined ? 1 : Number(rgba[4]);
    return {
      color: `rgb(${red}, ${green}, ${blue})`,
      opacity: Number.isFinite(opacity) ? Math.min(Math.max(opacity, 0), 1) : 1,
    };
  }

  const hex = value.match(/^#([0-9a-f]{6})([0-9a-f]{2})?$/i);
  if (hex) {
    return {
      color: `#${hex[1]}`,
      opacity: hex[2] === undefined ? 1 : parseInt(hex[2], 16) / 255,
    };
  }

  return { color: value, opacity: 1 };
}

/** Alpha-free colour for a material — use this instead of `themeColor` there. */
export function themeColorSolid(token: string, fallback: string): string {
  return splitCssColorAlpha(resolveToken(token, fallback)).color;
}

/** Colour plus alpha, for callers that drive `material.opacity` from the token. */
export function themeColorWithAlpha(
  token: string,
  fallback: string,
): ThemeColorParts {
  return splitCssColorAlpha(resolveToken(token, fallback));
}

export function themeColor(token: string, fallback: string) {
  return resolveToken(token, fallback);
}