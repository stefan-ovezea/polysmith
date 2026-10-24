// @vitest-environment node
// Config defaults that a saved file would otherwise never see.
//
// The constraint glyphs (H/V/âŠ¥/âˆ¥) used to ship hidden, so every existing install
// stored `showConstraints: false` and never showed a constraint at all â€” the
// sketch reported "2 DOF" with nothing on screen explaining it. The default is
// now `true`, and the config schema version makes that reach the installs that
// already saved the old default once, while a deliberate "off" (saved after v2)
// still wins.
import { beforeEach, describe, expect, it } from "vitest";

const CONFIG_STORAGE_KEY = "polysmith.appConfig";

function stubStorage(initial: string | null) {
  let stored = initial;
  (globalThis as unknown as { window: unknown }).window = {
    localStorage: {
      getItem: () => stored,
      setItem: (_key: string, value: string) => {
        stored = value;
      },
      removeItem: () => {
        stored = null;
      },
    },
    matchMedia: () => ({ matches: false }),
  };
  return {
    read: () => stored,
  };
}

describe("app config migration", () => {
  beforeEach(() => {
    delete (globalThis as unknown as { window?: unknown }).window;
  });

  it("turns constraint glyphs on for a config saved before the default changed", async () => {
    stubStorage(
      JSON.stringify({
        theme: "dark",
        viewport: {
          crosshair: "default",
          showGrid: true,
          showSketchGrid: true,
          showConstraints: false,
        },
      }),
    );
    const { loadAppConfig } = await import("./appConfig");
    expect(loadAppConfig().viewport.showConstraints).toBe(true);
  });

  it("keeps a deliberate 'off' once the file is at the current version", async () => {
    stubStorage(
      JSON.stringify({
        configVersion: 2,
        theme: "dark",
        viewport: {
          crosshair: "default",
          showGrid: true,
          showSketchGrid: true,
          showConstraints: false,
        },
      }),
    );
    const { loadAppConfig } = await import("./appConfig");
    expect(loadAppConfig().viewport.showConstraints).toBe(false);
  });

  it("ships the glyphs on for a fresh install", async () => {
    stubStorage(null);
    const { loadAppConfig } = await import("./appConfig");
    expect(loadAppConfig().viewport.showConstraints).toBe(true);
  });

  it("does not lose the rest of the stored config while migrating", async () => {
    stubStorage(
      JSON.stringify({
        theme: "light",
        displayUnits: "in",
        viewport: { crosshair: "infinite", showConstraints: false },
      }),
    );
    const { loadAppConfig } = await import("./appConfig");
    const config = loadAppConfig();
    expect(config.theme).toBe("light");
    expect(config.displayUnits).toBe("in");
    expect(config.viewport.crosshair).toBe("infinite");
    expect(config.viewport.showConstraints).toBe(true);
  });
});
