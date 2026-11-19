// The face table is packed on the wire (interned strings, flat binary arrays)
// and unpacked once at the boundary by the schema transform. If the unpacking
// is wrong the app still renders — the bodies come from the mesh — but face ids,
// sketch-on-face frames and CAM cylinder witnesses all go silently wrong.
import { describe, expect, it } from "vitest";

import { decodeSolidFaces } from "./binaryDecode";
import type { ViewportSolidFaces } from "@/types";

const f32 = (values: number[]) =>
  Buffer.from(new Float32Array(values).buffer).toString("base64");
const u32 = (values: number[]) =>
  Buffer.from(new Uint32Array(values).buffer).toString("base64");

function packed(): ViewportSolidFaces {
  // Three faces of one body: a planar face with an override colour, a selected
  // planar face without one, and a cylinder wall carrying the drill witness.
  const frame = [
    1, 2, 3, 1, 0, 0, 0, 1, 0, 0, 0, 1, // face 0
    10, 20, 30, 0, 1, 0, 0, 0, 1, 1, 0, 0, // face 1
    0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, -1, // face 2
  ];
  return {
    count: 3,
    owner_ids: ["feature-7"],
    owner_kinds: ["extrude"],
    sketchabilities: ["planar", "non-planar"],
    surface_kinds: ["planar", "cylinder"],
    colors: ["#ff0000"],
    owner: u32([0, 0, 0]),
    ordinal: u32([0, 5, 9]),
    sketchability: u32([0, 0, 1]),
    surface_kind: u32([0, 0, 1]),
    appearance: u32([0, 0xffffffff, 0xffffffff]),
    flags: u32([0, 1, 2]),
    center: f32([0, 0, 0, 0, 0, 0, 0, 0, 0]),
    normal: f32([0, 0, 1, 0, 0, 1, 1, 0, 0]),
    frame: f32(frame),
    size: f32([0, 0, 0, 0, 0, 0, 0, 0, 0]),
    cylinder: f32([1, 0, 0, 2, 3, 4, 5.5]),
  };
}

describe("decodeSolidFaces", () => {
  const faces = decodeSolidFaces(packed());

  it("rebuilds the face id and label from owner and ordinal", () => {
    expect(faces.map((face) => face.face_id)).toEqual([
      "feature-7:face:0",
      "feature-7:face:5",
      "feature-7:face:9",
    ]);
    expect(faces.map((face) => face.label)).toEqual([
      "Face 1",
      "Face 6",
      "Face 10",
    ]);
    expect(faces.map((face) => face.owner_kind)).toEqual([
      "extrude",
      "extrude",
      "extrude",
    ]);
  });

  it("unpacks the plane frame as vectors", () => {
    expect(faces[0].plane_frame.origin).toEqual({ x: 1, y: 2, z: 3 });
    expect(faces[0].plane_frame.x_axis).toEqual({ x: 1, y: 0, z: 0 });
    expect(faces[0].plane_frame.y_axis).toEqual({ x: 0, y: 1, z: 0 });
    expect(faces[0].plane_frame.normal).toEqual({ x: 0, y: 0, z: 1 });
    expect(faces[2].plane_frame.normal).toEqual({ x: 0, y: 0, z: -1 });
  });

  it("interns the appearance colour and keeps null distinct", () => {
    expect(faces[0].appearance_color).toBe("#ff0000");
    expect(faces[1].appearance_color).toBeNull();
    expect(faces[2].appearance_color).toBeNull();
  });

  it("reads selection and the cylinder witness from the flags", () => {
    expect(faces.map((face) => face.is_selected)).toEqual([false, true, false]);
    expect(faces[0].cylinder_axis).toBeNull();
    expect(faces[0].cylinder_radius).toBeNull();
    expect(faces[2].cylinder_axis).toEqual({ x: 1, y: 0, z: 0 });
    expect(faces[2].cylinder_location).toEqual({ x: 2, y: 3, z: 4 });
    expect(faces[2].cylinder_radius).toBeCloseTo(5.5, 5);
  });

  it("derives sketchability and surface kind from the interned enums", () => {
    expect(faces.map((face) => face.sketchability)).toEqual([
      "planar",
      "planar",
      "non-planar",
    ]);
    expect(faces.map((face) => face.surface_kind)).toEqual([
      "planar",
      "planar",
      "cylinder",
    ]);
  });

  it("returns nothing for an empty table", () => {
    expect(
      decodeSolidFaces({ ...packed(), count: 0, owner_ids: [] }),
    ).toEqual([]);
  });
});
