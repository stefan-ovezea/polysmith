import { describe, expect, it } from "vitest";

import {
  componentAncestors,
  componentBodyIds,
  componentHierarchy,
  importBodyGroups,
  importGroupFileLabel,
  isImportedBody,
} from "./documentHierarchyGroups";

// An imported file is ONE entity in the Browser: Fusion and SolidCraft both list
// the import as a node whose children are its bodies, and the Bodies category
// shows only modelled bodies. Without this the ESP32 carrier's 1,092 bodies were
// 1,092 rows under Bodies and the import itself had no entry at all.
function importedBody(
  id: string,
  operationId: string,
  summary = "ESP32-P4C5-Module_Carrier_V1.5.step · 120 faces",
) {
  return {
    feature_id: id,
    kind: "step_import",
    name: "Open CASCADE STEP translator 7.5 1.1.1",
    operation_id: operationId,
    parameters_summary: summary,
  };
}

describe("browser import grouping", () => {
  it("folds a whole imported assembly into one node", () => {
    const bodies = Array.from({ length: 1092 }, (_, index) =>
      importedBody(`feature-${index + 2}`, "feature-2"),
    );
    const groups = importBodyGroups(bodies);
    expect(groups).toHaveLength(1);
    expect(groups[0].operationId).toBe("feature-2");
    expect(groups[0].primary.feature_id).toBe("feature-2");
    expect(groups[0].members).toHaveLength(1092);
    // The first body is named after the STEP translator; the node shows the file.
    expect(groups[0].label).toBe("ESP32-P4C5-Module_Carrier_V1.5.step");
  });

  it("keeps two imports apart and ignores other features", () => {
    const groups = importBodyGroups([
      importedBody("feature-2", "feature-2", "board.step · 20 faces"),
      importedBody("feature-3", "feature-2", "board.step · 20 faces"),
      {
        feature_id: "feature-4",
        kind: "extrude",
        name: "Body",
        operation_id: "",
      },
      importedBody("feature-9", "feature-9", "lid.iges · 4 faces"),
    ]);
    expect(groups.map((group) => group.label)).toEqual([
      "board.step",
      "lid.iges",
    ]);
    expect(groups.map((group) => group.members.length)).toEqual([2, 1]);
  });

  it("gives a standalone import its own node", () => {
    const groups = importBodyGroups([
      { ...importedBody("feature-7", ""), kind: "iges_import" },
    ]);
    expect(groups).toHaveLength(1);
    expect(groups[0].operationId).toBe("feature-7");
    expect(groups[0].members).toHaveLength(1);
  });

  it("treats an imported body as part of its import, not of Bodies", () => {
    expect(isImportedBody({ kind: "step_import" })).toBe(true);
    expect(isImportedBody({ kind: "iges_import" })).toBe(true);
    expect(isImportedBody({ kind: "extrude" })).toBe(false);
    expect(isImportedBody({ kind: "mesh_import" })).toBe(false);
  });

  it("falls back to a generic label when the summary names no file", () => {
    expect(importGroupFileLabel([{ parameters_summary: "" }])).toBeNull();
    expect(
      importGroupFileLabel([{ parameters_summary: "Open CASCADE 1.1.1 · 2" }]),
    ).toBeNull();
    expect(
      importGroupFileLabel([
        { parameters_summary: "C:\\parts\\bracket.stp · 8 faces" },
      ]),
    ).toBe("C:\\parts\\bracket.stp");
  });
});

// A STEP assembly carries its own hierarchy: component OCCURRENCES ("U1:1"), not
// just parts. The Browser renders that tree, so the grouping must nest by parent
// id, scope it to one import, and drop occurrences that lead nowhere.
function component(
  id: string,
  name: string,
  parentId: string,
  bodyIds: string[],
) {
  return { component_id: id, name, parent_id: parentId, body_ids: bodyIds };
}

describe("browser component tree", () => {
  const components = [
    component("component-1", "Board:1", "", []),
    component("component-2", "U1:1", "component-1", []),
    component("component-3", "QFN-104:1", "component-2", ["feature-3", "feature-4"]),
    component("component-4", "U2:1", "component-1", ["feature-5"]),
  ];

  it("nests occurrences by parent id", () => {
    const { forest, placed } = componentHierarchy(components, [
      "feature-3",
      "feature-4",
      "feature-5",
    ]);
    expect(forest).toHaveLength(1);
    expect(forest[0].component.name).toBe("Board:1");
    expect(forest[0].children.map((node) => node.component.name)).toEqual([
      "U1:1",
      "U2:1",
    ]);
    expect(forest[0].children[0].children[0].component.name).toBe("QFN-104:1");
    expect(forest[0].children[0].children[0].bodyIds).toEqual([
      "feature-3",
      "feature-4",
    ]);
    expect([...placed].sort()).toEqual(["feature-3", "feature-4", "feature-5"]);
  });

  it("lists a component's whole subtree of bodies", () => {
    const { forest } = componentHierarchy(components, [
      "feature-3",
      "feature-4",
      "feature-5",
    ]);
    expect(componentBodyIds(forest[0]).sort()).toEqual([
      "feature-3",
      "feature-4",
      "feature-5",
    ]);
  });

  it("scopes the tree to one import", () => {
    const { forest, placed } = componentHierarchy(components, ["feature-5"]);
    // QFN-104:1 places nothing of this import, so it and the occurrence that
    // only led to it are gone; Board:1 and U2:1 stay.
    expect(forest).toHaveLength(1);
    expect(forest[0].children.map((node) => node.component.name)).toEqual([
      "U2:1",
    ]);
    expect([...placed]).toEqual(["feature-5"]);
    expect(componentHierarchy(components, []).forest).toHaveLength(0);
  });

  it("returns no tree for a part file without occurrences", () => {
    expect(componentHierarchy([], ["feature-2"]).forest).toHaveLength(0);
  });

  it("names the occurrence chain that holds a body, outermost first", () => {
    expect(componentAncestors(components, "feature-3")).toEqual([
      "component-1",
      "component-2",
      "component-3",
    ]);
    expect(componentAncestors(components, "feature-5")).toEqual([
      "component-1",
      "component-4",
    ]);
    // A body no component places (a surface body) has no chain to open.
    expect(componentAncestors(components, "feature-99")).toEqual([]);
  });
});
