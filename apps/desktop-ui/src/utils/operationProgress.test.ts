import { describe, expect, it } from "vitest";

import {
  elapsedSeconds,
  fileNameFromPath,
  parseOperationProgress,
} from "./operationProgress";

describe("fileNameFromPath", () => {
  it("takes the trailing segment of a Windows path", () => {
    expect(
      fileNameFromPath(
        "C:\\SRC\\devboard-esp32p4c5_module\\docs\\ESP32-P4C5-Module_Carrier_V1.5.step",
      ),
    ).toBe("ESP32-P4C5-Module_Carrier_V1.5.step");
  });

  it("takes the trailing segment of a POSIX path", () => {
    expect(fileNameFromPath("/home/me/models/board.step")).toBe("board.step");
  });

  it("returns a bare name unchanged", () => {
    expect(fileNameFromPath("board.step")).toBe("board.step");
  });

  it("does not return an empty string when the path ends in a separator", () => {
    expect(fileNameFromPath("C:\\SRC\\models\\")).toBe("models");
  });

  it("returns an empty string for an empty path", () => {
    expect(fileNameFromPath("   ")).toBe("");
  });
});

describe("elapsedSeconds", () => {
  it("counts whole seconds", () => {
    expect(elapsedSeconds(1_000, 1_999)).toBe(0);
    expect(elapsedSeconds(1_000, 2_000)).toBe(1);
    expect(elapsedSeconds(1_000, 17_500)).toBe(16);
  });

  it("never goes negative when the clock steps backwards", () => {
    expect(elapsedSeconds(5_000, 4_000)).toBe(0);
  });

  it("returns zero for missing timestamps", () => {
    expect(elapsedSeconds(Number.NaN, 1_000)).toBe(0);
    expect(elapsedSeconds(1_000, Number.NaN)).toBe(0);
  });
});

// The overlay shows a determinate bar only for the phase the core can count.
// Everything else must stay indeterminate rather than invent a fraction.
describe("parseOperationProgress", () => {
  it("reads the body-build counter", () => {
    expect(parseOperationProgress("Writing part 273/1092")).toEqual({
      current: 273,
      total: 1092,
      fraction: 273 / 1092,
    });
  });

  it("tolerates spacing and case", () => {
    expect(parseOperationProgress("writing part 5/10")?.fraction).toBeCloseTo(0.5);
  });

  it("reports no fraction for an opaque stage", () => {
    expect(parseOperationProgress("Reading 'board.step' ...")).toBeNull();
    expect(parseOperationProgress("Transfer into the document in 6167 ms")).toBeNull();
    expect(parseOperationProgress("")).toBeNull();
  });

  it("never divides by zero and clamps a runaway counter", () => {
    expect(parseOperationProgress("Writing part 3/0")).toBeNull();
    const clamped = parseOperationProgress("Writing part 2000/1000");
    expect(clamped?.current).toBe(1000);
    expect(clamped?.fraction).toBe(1);
  });
});
