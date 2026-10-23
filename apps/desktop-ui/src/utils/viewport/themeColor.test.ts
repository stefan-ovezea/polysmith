// @vitest-environment node
// D-002: the theme-token → material-colour boundary.
//
// `themeColor()` returns the CSS string verbatim (right for CSS/SVG/canvas 2D).
// THREE.Color silently DROPS the alpha of `rgba(...)`, so a token that carries
// alpha has to be split here, or the intended translucency is lost and Three
// logs "Alpha component ... will be ignored" once per string.
import { describe, expect, it } from "vitest";

import { splitCssColorAlpha } from "./themeColor";

describe("splitCssColorAlpha", () => {
  it("splits rgba into an alpha-free colour and the alpha", () => {
    expect(splitCssColorAlpha("rgba(186, 194, 222, 0.82)")).toEqual({
      color: "rgb(186, 194, 222)",
      opacity: 0.82,
    });
  });

  it("treats rgb as fully opaque", () => {
    expect(splitCssColorAlpha("rgb(10, 20, 30)")).toEqual({
      color: "rgb(10, 20, 30)",
      opacity: 1,
    });
  });

  it("strips the alpha of an eight-digit hex", () => {
    const parts = splitCssColorAlpha("#8feaf7d1");
    expect(parts.color).toBe("#8feaf7");
    expect(parts.opacity).toBeCloseTo(209 / 255, 6);
  });

  it("leaves a six-digit hex and a plain name alone", () => {
    expect(splitCssColorAlpha("#8feaf7")).toEqual({
      color: "#8feaf7",
      opacity: 1,
    });
    expect(splitCssColorAlpha("transparent")).toEqual({
      color: "transparent",
      opacity: 1,
    });
  });

  it("clamps an out-of-range alpha and ignores a non-numeric one", () => {
    expect(splitCssColorAlpha("rgba(1, 2, 3, 1.5)").opacity).toBe(1);
    expect(splitCssColorAlpha("rgba(1, 2, 3, 0)").opacity).toBe(0);
    expect(splitCssColorAlpha("rgba(1, 2, 3, x)").opacity).toBe(1);
  });
});