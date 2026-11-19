// Decode the binary viewport geometry blobs (phase E4).  The core ships mesh
// positions/normals as base64 float32 and indices as base64 uint32; these decode
// straight into typed arrays so the renderer can hand them to BufferGeometry
// without a parse-and-widen pass.

import type { ViewportSolidFace, ViewportSolidFaces } from "@/types";

function decodeBase64(base64: string): Uint8Array {
  if (!base64) {
    return new Uint8Array(0);
  }
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}

export function decodeF32(base64: string): Float32Array {
  const bytes = decodeBase64(base64);
  return new Float32Array(bytes.buffer, 0, Math.floor(bytes.byteLength / 4));
}

export function decodeU32(base64: string): Uint32Array {
  const bytes = decodeBase64(base64);
  return new Uint32Array(bytes.buffer, 0, Math.floor(bytes.byteLength / 4));
}

// Narrow mesh encodings (see the core's mesh emitter): uint16 indices and pick
// map, int8 normals. Each is exact for the data it carries - a body's vertex or
// face ordinal fits 16 bits and a unit normal only needs direction.
export function decodeU16(base64: string): Uint16Array {
  const bytes = decodeBase64(base64);
  return new Uint16Array(bytes.buffer, 0, Math.floor(bytes.byteLength / 2));
}

export function decodeI8(base64: string): Int8Array {
  const bytes = decodeBase64(base64);
  return new Int8Array(bytes.buffer, 0, bytes.byteLength);
}

const NO_APPEARANCE = 0xffffffff;

/**
 * Unpack the face table back into per-face records.
 *
 * The wire interns the strings and flattens the numbers because a face used to
 * cost ~670 B (9.54 MB on a 14,897-face board, most of it repeated ids and the
 * keyed plane frame). `face_id` and `label` are rebuilt from the owner and the
 * ordinal, which is exactly what the core used to send.
 */
export function decodeSolidFaces(
  packed: ViewportSolidFaces,
): ViewportSolidFace[] {
  const count = packed.count ?? 0;
  if (count === 0) {
    return [];
  }
  const owner = decodeU32(packed.owner);
  const ordinal = decodeU32(packed.ordinal);
  const sketchability = decodeU32(packed.sketchability);
  const surfaceKind = decodeU32(packed.surface_kind);
  const appearance = decodeU32(packed.appearance);
  const flags = decodeU32(packed.flags);
  const center = decodeF32(packed.center);
  const normal = decodeF32(packed.normal);
  const frame = decodeF32(packed.frame);
  const size = decodeF32(packed.size);
  const cylinder = decodeF32(packed.cylinder);

  const faces: ViewportSolidFace[] = [];
  let cylinderIndex = 0;
  for (let index = 0; index < count; index += 1) {
    const ownerId = packed.owner_ids[owner[index]] ?? "";
    const faceOrdinal = ordinal[index] ?? 0;
    const hasCylinder = ((flags[index] ?? 0) & 2) !== 0;
    const cylinderAxis = hasCylinder
      ? {
          x: cylinder[cylinderIndex],
          y: cylinder[cylinderIndex + 1],
          z: cylinder[cylinderIndex + 2],
        }
      : null;
    const cylinderLocation = hasCylinder
      ? {
          x: cylinder[cylinderIndex + 3],
          y: cylinder[cylinderIndex + 4],
          z: cylinder[cylinderIndex + 5],
        }
      : null;
    const cylinderRadius = hasCylinder ? cylinder[cylinderIndex + 6] : null;
    if (hasCylinder) {
      cylinderIndex += 7;
    }
    const colorIndex = appearance[index] ?? NO_APPEARANCE;
    const vec = (offset: number) => ({
      x: frame[offset],
      y: frame[offset + 1],
      z: frame[offset + 2],
    });
    faces.push({
      face_id: `${ownerId}:face:${faceOrdinal}`,
      owner_id: ownerId,
      owner_kind: packed.owner_kinds[owner[index]] ?? "",
      label: `Face ${faceOrdinal + 1}`,
      sketchability: packed.sketchabilities[sketchability[index]] ?? "planar",
      center: { x: center[index * 3], y: center[index * 3 + 1], z: center[index * 3 + 2] },
      normal: { x: normal[index * 3], y: normal[index * 3 + 1], z: normal[index * 3 + 2] },
      plane_frame: {
        origin: vec(index * 12),
        x_axis: vec(index * 12 + 3),
        y_axis: vec(index * 12 + 6),
        normal: vec(index * 12 + 9),
      },
      size: {
        width: size[index * 3],
        height: size[index * 3 + 1],
        radius: size[index * 3 + 2],
      },
      is_selected: ((flags[index] ?? 0) & 1) !== 0,
      appearance_color:
        colorIndex === NO_APPEARANCE ? null : (packed.colors[colorIndex] ?? null),
      surface_kind: packed.surface_kinds[surfaceKind[index]] ?? "other",
      cylinder_axis: cylinderAxis,
      cylinder_location: cylinderLocation,
      cylinder_radius: cylinderRadius,
    });
  }
  return faces;
}


// Faces no longer ship their own triangles: the body mesh carries `face_ids`
// (one face ordinal per triangle), so each face's pick proxy is carved out of
// the mesh the renderer already has. The proxies are what the picker and the
// highlight layer use, so this keeps the interaction code unchanged while
// deleting a second copy of every triangle from the wire (~3 MB on the 28 MB
// board, ~11 MB on the 1,092-body carrier).
