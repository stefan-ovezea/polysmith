// The schema is the boundary: a field the core sends but zod does not declare is
// silently STRIPPED, and the UI then degrades to a fallback path. That is how
// face picking broke: `meshes[].face_ids` was missing here, so every face proxy
// fell back to a flat plane-frame rectangle and clicks resolved to the wrong
// face (reported 2026-11-20 as "make sketch on the top, sketch on the side").
import { describe, expect, it } from "vitest";

import {
  viewportMeshSchema,
  viewportSolidFacesSchema,
} from "./viewportStateSchema";

function mesh(extra: Record<string, unknown> = {}) {
  return {
    primitive_id: "feature-1",
    positions: "",
    normals: "",
    indices: "",
    is_selected: false,
    ...extra,
  };
}

describe("viewportStateSchema mesh fields", () => {
  it("keeps the per-triangle pick map", () => {
    const parsed = viewportMeshSchema.parse(mesh({ face_ids: "AAAA" }));
    expect(parsed.face_ids).toBe("AAAA");
  });

  it("defaults the pick map to empty so an older core still parses", () => {
    const parsed = viewportMeshSchema.parse(mesh());
    expect(parsed.face_ids).toBe("");
  });

  it("parses the scene-unchanged face table that carries only a count", () => {
    expect(() => viewportSolidFacesSchema.parse({ count: 0 })).not.toThrow();
    expect(viewportSolidFacesSchema.parse({ count: 0 })).toEqual([]);
  });
});
