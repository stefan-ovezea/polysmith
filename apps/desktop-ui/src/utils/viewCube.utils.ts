import * as THREE from "three";
import { themeColor } from "./viewport.utils";

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export type CubeFace = "TOP" | "BOTTOM" | "FRONT" | "BACK" | "LEFT" | "RIGHT";

export type ViewCubeHit =
  | { type: "face"; face: CubeFace }
  | { type: "edge"; face1: CubeFace; face2: CubeFace }
  | { type: "corner"; faces: [CubeFace, CubeFace, CubeFace] }
  | { type: "rotation_arrow"; direction: -1 | 1 }
  | {
      type: "orbit_arrow";
      /** `up`: around the vertical (east/west); `right`: around the horizontal (north/south). */
      axis: "up" | "right";
      direction: -1 | 1;
    }
  | { type: "button"; action: CubeButton }
  | null;

/** The corner buttons around the cube. */
export type CubeButton = "home" | "fit" | "backside" | "menu";

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const CUBE_SIZE = 1;
// FreeCAD 1.1's NaviCube chamfers the cube by 0.12 of its size
// (`NaviCubeImplementation::chamfer`, settable 0.05-0.18). The chamfer is what
// gives it the soft silhouette and the triangles at the corners.
const CUBE_CHAMFER = 0.12;
// The widget is OPAQUE. It started at FreeCAD's half opacity, but a transparent
// cube shows the far faces' labels through the near ones (BOTTOM read backwards
// through FRONT), which looks like a rendering fault rather than a subtle
// control. Hover still lights the picked facet; it no longer changes the body's
// strength.
const INACTIVE_OPACITY = 1;
const CUBE_VIEWPORT_SIZE = 168;
const CUBE_VIEWPORT_MARGIN = 16;
const ANIMATION_DURATION = 300;

const FACE_NORMALS: Record<CubeFace, THREE.Vector3> = {
  TOP: new THREE.Vector3(0, 0, 1),
  BOTTOM: new THREE.Vector3(0, 0, -1),
  FRONT: new THREE.Vector3(0, -1, 0),
  BACK: new THREE.Vector3(0, 1, 0),
  RIGHT: new THREE.Vector3(1, 0, 0),
  LEFT: new THREE.Vector3(-1, 0, 0),
};

// The world axis each face looks ALONG, Z-up. The face name answers "which view
// is this", the letters answer "which way is that in the model's coordinates" —
// the second thing SolidWorks' reference triad and SolidCraft's axis labels both
// give, and the one a user coming from either expects to read off the cube.
const FACE_AXIS_LABELS: Record<CubeFace, string> = {
  TOP: "Z+",
  BOTTOM: "Z-",
  FRONT: "Y-",
  BACK: "Y+",
  RIGHT: "X+",
  LEFT: "X-",
};

/** The axis line printed under a face's view name, e.g. "Z+" for TOP. */
export function faceAxisLabel(face: CubeFace): string {
  return FACE_AXIS_LABELS[face];
}

function viewCubeColor(token: string, fallback: string) {
  return themeColor(token, fallback);
}

function faceColors(): Record<CubeFace, string> {
  return {
    RIGHT: viewCubeColor("--cad-viewcube-right", "#242323"),
    LEFT: viewCubeColor("--cad-viewcube-left", "#1f1f1f"),
    TOP: viewCubeColor("--cad-viewcube-top", "#2b2a2a"),
    BOTTOM: viewCubeColor("--cad-viewcube-bottom", "#1a1919"),
    FRONT: viewCubeColor("--cad-viewcube-front", "#262525"),
    BACK: viewCubeColor("--cad-viewcube-back", "#202020"),
  };
}

// ---------------------------------------------------------------------------
// Face label texture
// ---------------------------------------------------------------------------

function createFaceLabelTexture(
  label: string,
  axisLabel: string,
  bgColor: string,
): THREE.CanvasTexture {
  const size = 256;
  const canvas = document.createElement("canvas");
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext("2d")!;

  const gradient = ctx.createLinearGradient(0, 0, size, size);
  gradient.addColorStop(0, bgColor);
  gradient.addColorStop(
    1,
    viewCubeColor("--cad-viewcube-gradient-end", "#141414"),
  );
  ctx.fillStyle = gradient;
  ctx.fillRect(0, 0, size, size);

  ctx.strokeStyle = viewCubeColor("--cad-viewcube-border", "#3b494c");
  ctx.lineWidth = 10;
  ctx.strokeRect(8, 8, size - 16, size - 16);

  ctx.fillStyle = viewCubeColor("--cad-viewcube-label", "#e5e2e1");
  ctx.font = `700 38px "Space Grotesk", "Inter", sans-serif`;
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillText(label, size / 2, size / 2 - 16);

  // The world axis this face looks along, in SolidCraft's convention: face
  // names are views, the axis letters are the world axes
  // (docs/control-protocol.md: "Face names follow the up-axis preference; the
  // small X/Y/Z triad shows world axes"). One line per face is the cheap half of
  // that: the cube says WHERE "Top" is, in X/Y/Z, without adding a second widget.
  ctx.fillStyle = viewCubeColor("--cad-viewcube-axis-label", "#9fb0b3");
  ctx.font = `600 26px "Space Grotesk", "Inter", sans-serif`;
  ctx.fillText(axisLabel, size / 2, size / 2 + 34);

  const texture = new THREE.CanvasTexture(canvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  texture.needsUpdate = true;
  return texture;
}

// A button glyph for the corner controls: a house (Home = isometric + fit),
// four brackets (Fit = frame the model without changing the direction) and a
// double-headed flip mark (Backside = look from the other side).
function createCubeButtonTexture(button: CubeButton): THREE.CanvasTexture {
  const size = 128;
  const canvas = document.createElement("canvas");
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext("2d")!;
  ctx.clearRect(0, 0, size, size);
  ctx.strokeStyle = "#ffffff";
  ctx.fillStyle = "#ffffff";
  ctx.lineWidth = 11;
  ctx.lineCap = "round";
  ctx.lineJoin = "round";

  if (button === "home") {
    ctx.beginPath();
    ctx.moveTo(22, 62);
    ctx.lineTo(64, 26);
    ctx.lineTo(106, 62);
    ctx.stroke();
    ctx.strokeRect(38, 60, 52, 44);
    ctx.beginPath();
    ctx.moveTo(56, 104);
    ctx.lineTo(56, 78);
    ctx.lineTo(74, 78);
    ctx.lineTo(74, 104);
    ctx.stroke();
  } else if (button === "fit") {
    const arm = 30;
    const corners: Array<[number, number, number, number]> = [
      [26, 26, 1, 1],
      [102, 26, -1, 1],
      [26, 102, 1, -1],
      [102, 102, -1, -1],
    ];
    for (const [x, y, dx, dy] of corners) {
      ctx.beginPath();
      ctx.moveTo(x + dx * arm, y);
      ctx.lineTo(x, y);
      ctx.lineTo(x, y + dy * arm);
      ctx.stroke();
    }
  } else if (button === "backside") {
    // Backside: a circular flip arrow.
    ctx.beginPath();
    ctx.arc(64, 64, 34, Math.PI * 0.2, Math.PI * 1.55);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(30, 40);
    ctx.lineTo(44, 62);
    ctx.lineTo(60, 46);
    ctx.closePath();
    ctx.fill();
  } else {
    // Menu: three stacked lines.
    for (const y of [44, 64, 84]) {
      ctx.beginPath();
      ctx.moveTo(30, y);
      ctx.lineTo(98, y);
      ctx.stroke();
    }
  }

  const texture = new THREE.CanvasTexture(canvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  texture.needsUpdate = true;
  return texture;
}

// One of the four orbit arrows around the cube: a triangle pointing away from
// the widget, i.e. in the direction the camera will move.
function createOrbitArrowTexture(axis: "up" | "right", direction: -1 | 1): THREE.CanvasTexture {
  const size = 128;
  const canvas = document.createElement("canvas");
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext("2d")!;
  ctx.clearRect(0, 0, size, size);
  ctx.fillStyle = "#ffffff";

  // Draw pointing "up", then rotate into place: the glyph must point the way the
  // camera will move. North is `right:-1` (a negative turn about the eye's right
  // axis lifts it), south the opposite, east is `up:+1`, west `up:-1`.
  const rotations: Record<string, number> = {
    "right:-1": 0,
    "right:1": Math.PI,
    "up:1": Math.PI / 2,
    "up:-1": -Math.PI / 2,
  };
  ctx.save();
  ctx.translate(size / 2, size / 2);
  ctx.rotate(rotations[`${axis}:${direction}`]);
  ctx.beginPath();
  ctx.moveTo(0, -46);
  ctx.lineTo(34, 10);
  ctx.lineTo(0, -4);
  ctx.lineTo(-34, 10);
  ctx.closePath();
  ctx.fill();
  ctx.restore();

  const texture = new THREE.CanvasTexture(canvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  texture.needsUpdate = true;
  return texture;
}

function createRotationArrowTexture(direction: -1 | 1): THREE.CanvasTexture {
  const size = 256;
  const canvas = document.createElement("canvas");
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext("2d")!;

  ctx.clearRect(0, 0, size, size);
  ctx.lineCap = "butt";
  ctx.lineJoin = "round";

  if (direction > 0) {
    ctx.translate(size, 0);
    ctx.scale(-1, 1);
  }

  ctx.strokeStyle = viewCubeColor("--cad-viewcube-arrow", "#c3f5ff");
  ctx.fillStyle = viewCubeColor("--cad-viewcube-arrow", "#c3f5ff");
  ctx.lineWidth = 58;

  const center = 128;
  const radius = 88;
  ctx.beginPath();
  ctx.arc(center, center + 16, radius, Math.PI * 1.5, Math.PI, true);
  ctx.stroke();

  ctx.beginPath();
  ctx.moveTo(198, 38);
  ctx.lineTo(118, 0);
  ctx.lineTo(126, 106);
  ctx.closePath();
  ctx.fill();

  const texture = new THREE.CanvasTexture(canvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  texture.needsUpdate = true;
  return texture;
}

// ---------------------------------------------------------------------------
// Chamfered-cube facets
// ---------------------------------------------------------------------------
//
// FreeCAD 1.1's NaviCube does not put a box corner on each vertex: it CHAMFERS
// the cube (`NaviCubeImplementation::chamfer`, 0.12 of the size by default,
// settable 0.05-0.18), which turns each of the 8 corners into a triangle and
// each of the 12 edges into a strip. Those facets are the pick targets, so the
// widget reads as one soft solid instead of a box with knobs on it.
//
// Same here: 6 face quads (inset by the chamfer), 12 edge strips, 8 corner
// triangles = the 26 views, and the corner you grab is the triangle between
// three faces rather than a sphere sitting on the point.

export interface CubeFacet {
  kind: "face" | "edge" | "corner";
  /** The 1, 2 or 3 cube faces this facet lies between. */
  faces: CubeFace[];
  /** Quad (4) or triangle (3) in cube-local space, half size = CUBE_SIZE / 2. */
  vertices: Array<[number, number, number]>;
  /** Optional texture coordinates, present for face facets (labels). */
  uvs?: Array<[number, number]>;
}

// Per face: the outward normal plus the 2D basis the label is drawn in, so the
// text reads upright when that face looks at the viewer.
const FACE_BASIS: Record<
  CubeFace,
  {
    normal: [number, number, number];
    right: [number, number, number];
    up: [number, number, number];
  }
> = {
  RIGHT: { normal: [1, 0, 0], right: [0, -1, 0], up: [0, 0, 1] },
  LEFT: { normal: [-1, 0, 0], right: [0, 1, 0], up: [0, 0, 1] },
  BACK: { normal: [0, 1, 0], right: [-1, 0, 0], up: [0, 0, 1] },
  FRONT: { normal: [0, -1, 0], right: [1, 0, 0], up: [0, 0, 1] },
  TOP: { normal: [0, 0, 1], right: [1, 0, 0], up: [0, 1, 0] },
  BOTTOM: { normal: [0, 0, -1], right: [1, 0, 0], up: [0, 1, 0] },
};

const FACE_ORDER: CubeFace[] = [
  "RIGHT",
  "LEFT",
  "BACK",
  "FRONT",
  "TOP",
  "BOTTOM",
];

function scaled(
  vector: [number, number, number],
  factor: number,
): [number, number, number] {
  return [vector[0] * factor, vector[1] * factor, vector[2] * factor];
}

function added(
  a: [number, number, number],
  b: [number, number, number],
): [number, number, number] {
  return [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
}

function cross(
  a: [number, number, number],
  b: [number, number, number],
): [number, number, number] {
  return [
    a[1] * b[2] - a[2] * b[1],
    a[2] * b[0] - a[0] * b[2],
    a[0] * b[1] - a[1] * b[0],
  ];
}

function dot(a: [number, number, number], b: [number, number, number]): number {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/**
 * Order a facet's vertices counter-clockwise as seen from OUTSIDE.
 *
 * The label basis (right, up) is chosen so text reads upright, and for four of
 * the six faces right x up points INTO the cube — a quad authored in that order
 * faces inward, which makes a depth-sorted render draw the back of the cube in
 * front of the front. Reversing the list (and its UVs, so the label stays put)
 * fixes the winding without mirroring the text.
 */
function oriented(
  vertices: Array<[number, number, number]>,
  uvs: Array<[number, number]> | undefined,
  outward: [number, number, number],
): { vertices: Array<[number, number, number]>; uvs?: Array<[number, number]> } {
  const normal = cross(
    [
      vertices[1][0] - vertices[0][0],
      vertices[1][1] - vertices[0][1],
      vertices[1][2] - vertices[0][2],
    ],
    [
      vertices[2][0] - vertices[0][0],
      vertices[2][1] - vertices[0][1],
      vertices[2][2] - vertices[0][2],
    ],
  );
  if (dot(normal, outward) >= 0) {
    return { vertices, uvs };
  }
  return {
    vertices: [vertices[0], ...vertices.slice(1).reverse()],
    uvs: uvs ? [uvs[0], ...uvs.slice(1).reverse()] : undefined,
  };
}

/**
 * The 26 facets of a chamfered cube, in cube-local space.
 *
 * `chamfer` is a fraction of the cube size (FreeCAD's default is 0.12). At 0 the
 * faces meet at knife edges; at 0.5 the cube becomes a rhombicuboctahedron.
 */
export function chamferedCubeFacets(
  chamfer: number = CUBE_CHAMFER,
  half: number = CUBE_SIZE / 2,
): CubeFacet[] {
  const cut = THREE.MathUtils.clamp(chamfer, 0, 0.45) * (half * 2);
  const inner = Math.max(half - cut, 0.02);
  const facets: CubeFacet[] = [];

  // -- 6 face quads ---------------------------------------------------------
  for (const face of FACE_ORDER) {
    const { normal, right, up } = FACE_BASIS[face];
    const centre = scaled(normal, half);
    const bl = added(added(centre, scaled(right, -inner)), scaled(up, -inner));
    const br = added(added(centre, scaled(right, inner)), scaled(up, -inner));
    const tr = added(added(centre, scaled(right, inner)), scaled(up, inner));
    const tl = added(added(centre, scaled(right, -inner)), scaled(up, inner));
    const ordered = oriented(
      [bl, br, tr, tl],
      [
        [0, 0],
        [1, 0],
        [1, 1],
        [0, 1],
      ],
      normal,
    );
    facets.push({
      kind: "face",
      faces: [face],
      vertices: ordered.vertices,
      uvs: ordered.uvs,
    });
  }

  // -- 12 edge strips -------------------------------------------------------
  // Every pair of different axes, one sign each: the strip lies between those
  // two faces and runs along the third axis.
  for (const faceA of FACE_ORDER) {
    for (const faceB of FACE_ORDER) {
      const axisA = FACE_BASIS[faceA].normal.findIndex((value) => value !== 0);
      const axisB = FACE_BASIS[faceB].normal.findIndex((value) => value !== 0);
      if (axisA === axisB) {
        continue;
      }
      // One strip per unordered pair, and the faces must be adjacent: pick the
      // pair whose axes differ and whose remaining axis is not shared.
      if (fullIndex(faceA) > fullIndex(faceB)) {
        continue;
      }
      const axisC = 3 - axisA - axisB;
      const normalA = FACE_BASIS[faceA].normal;
      const normalB = FACE_BASIS[faceB].normal;
      const alongC: [number, number, number] = [0, 0, 0];
      alongC[axisC] = inner;

      const quad: Array<[number, number, number]> = [];
      for (const signC of [-1, 1] as const) {
        const cVec = scaled(alongC, signC);
        // Edge of the A face, on the B side, at +-inner along C.
        const onA: [number, number, number] = [0, 0, 0];
        onA[axisA] = normalA[axisA] * half;
        onA[axisB] = normalB[axisB] * inner;
        const onB: [number, number, number] = [0, 0, 0];
        onB[axisA] = normalA[axisA] * inner;
        onB[axisB] = normalB[axisB] * half;
        quad.push(added(added(onA, cVec), [0, 0, 0]));
        quad.push(added(added(onB, cVec), [0, 0, 0]));
      }
      // Outward normal of the strip: halfway between its two face normals.
      const outward: [number, number, number] = [
        normalA[0] + normalB[0],
        normalA[1] + normalB[1],
        normalA[2] + normalB[2],
      ];
      const orderedEdge = oriented(
        [quad[0], quad[1], quad[3], quad[2]],
        undefined,
        outward,
      );
      facets.push({
        kind: "edge",
        faces: [faceA, faceB],
        vertices: orderedEdge.vertices,
      });
    }
  }

  // -- 8 corner triangles ---------------------------------------------------
  for (const sx of [-1, 1] as const) {
    for (const sy of [-1, 1] as const) {
      for (const sz of [-1, 1] as const) {
        const signs = [sx, sy, sz];
        const faces = [
          signs[0] > 0 ? "RIGHT" : "LEFT",
          signs[1] > 0 ? "BACK" : "FRONT",
          signs[2] > 0 ? "TOP" : "BOTTOM",
        ] as CubeFace[];
        const triangle = [0, 1, 2].map((axis) => {
          const point: [number, number, number] = [0, 0, 0];
          for (let index = 0; index < 3; index += 1) {
            point[index] = signs[index] * (index === axis ? half : inner);
          }
          return point;
        });
        // Outward normal points along the cube diagonal this corner sits on.
        const outward: [number, number, number] = [sx, sy, sz];
        const orderedCorner = oriented(triangle, undefined, outward);
        facets.push({
          kind: "corner",
          faces,
          vertices: orderedCorner.vertices,
        });
      }
    }
  }

  return facets;
}

function fullIndex(face: CubeFace): number {
  return FACE_ORDER.indexOf(face);
}

function facetGeometry(facet: CubeFacet): THREE.BufferGeometry {
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute(
    "position",
    new THREE.BufferAttribute(new Float32Array(facet.vertices.flat()), 3),
  );
  if (facet.uvs) {
    geometry.setAttribute(
      "uv",
      new THREE.BufferAttribute(new Float32Array(facet.uvs.flat()), 2),
    );
  }
  geometry.setIndex(
    facet.vertices.length === 4 ? [0, 1, 2, 0, 2, 3] : [0, 1, 2],
  );
  return geometry;
}

// ---------------------------------------------------------------------------
// Build the view cube group (26 chamfer facets + roll arrows)
// ---------------------------------------------------------------------------

export function buildViewCubeGroup(): THREE.Group {
  const group = new THREE.Group();
  const colors = faceColors();
  const edgeColor = viewCubeColor("--cad-viewcube-edge", "#2d3436");
  const cornerColor = viewCubeColor("--cad-viewcube-corner", "#3a4143");
  const arrowMutedColor = viewCubeColor("--cad-viewcube-arrow-muted", "#5d6d70");

  for (const facet of chamferedCubeFacets()) {
    let material: THREE.MeshBasicMaterial;
    const userData: Record<string, unknown> = {
      cubePart: true,
      cubeType: facet.kind,
      baseColor: edgeColor,
    };
    if (facet.kind === "face") {
      const face = facet.faces[0];
      material = new THREE.MeshBasicMaterial({
        map: createFaceLabelTexture(face, faceAxisLabel(face), colors[face]),
        color: viewCubeColor("--cad-viewcube-material", "#ffffff"),
        transparent: true,
        opacity: INACTIVE_OPACITY,
        side: THREE.DoubleSide,
      });
      userData.face = face;
      userData.baseColor = viewCubeColor("--cad-viewcube-material", "#ffffff");
    } else {
      const colour = facet.kind === "edge" ? edgeColor : cornerColor;
      material = new THREE.MeshBasicMaterial({
        color: colour,
        transparent: true,
        opacity: INACTIVE_OPACITY,
        side: THREE.DoubleSide,
      });
      userData.baseColor = colour;
      if (facet.kind === "edge") {
        userData.face1 = facet.faces[0];
        userData.face2 = facet.faces[1];
      } else {
        userData.faces = facet.faces;
      }
    }
    const mesh = new THREE.Mesh(facetGeometry(facet), material);
    mesh.userData = userData;
    mesh.renderOrder = facet.kind === "face" ? 0 : 1;
    group.add(mesh);
  }

  for (const direction of [-1, 1] as const) {
    const texture = createRotationArrowTexture(direction);
    const material = new THREE.SpriteMaterial({
      map: texture,
      color: arrowMutedColor,
      transparent: true,
      opacity: 0.92,
      depthTest: false,
      depthWrite: false,
    });
    const sprite = new THREE.Sprite(material);
    sprite.scale.set(0.62, 0.47, 1);
    sprite.visible = false;
    sprite.renderOrder = 10;
    sprite.userData = {
      cubePart: true,
      cubeType: "rotation_arrow",
      direction,
      baseColor: arrowMutedColor,
      baseOpacity: 0.92,
    };
    group.add(sprite);
  }

  for (const action of ["home", "fit", "backside", "menu"] as const) {
    const material = new THREE.SpriteMaterial({
      map: createCubeButtonTexture(action),
      color: arrowMutedColor,
      transparent: true,
      opacity: 0.9,
      depthTest: false,
      depthWrite: false,
    });
    const sprite = new THREE.Sprite(material);
    // Positioned in CAMERA space by updateSketchRotationArrows each frame, so the
    // buttons stay in the widget's corners whatever the view does. Hidden until
    // that runs: the GRBL preview reuses this group for orientation only and
    // never places them.
    sprite.scale.set(0.4, 0.4, 1);
    sprite.visible = false;
    sprite.renderOrder = 11;
    sprite.userData = {
      cubePart: true,
      cubeType: "button",
      action,
      baseColor: arrowMutedColor,
      baseOpacity: 0.9,
    };
    group.add(sprite);
  }

  // Four orbit arrows: one per screen axis, "the camera moves the way the arrow
  // points" (FreeCAD's ArrowNorth/South/East/West).
  for (const axis of ["up", "right"] as const) {
    for (const direction of [-1, 1] as const) {
      const material = new THREE.SpriteMaterial({
        map: createOrbitArrowTexture(axis, direction),
        color: arrowMutedColor,
        transparent: true,
        opacity: 0.85,
        depthTest: false,
        depthWrite: false,
      });
      const sprite = new THREE.Sprite(material);
      sprite.scale.set(0.32, 0.32, 1);
      sprite.visible = false;
      sprite.renderOrder = 11;
      sprite.userData = {
        cubePart: true,
        cubeType: "orbit_arrow",
        axis,
        direction,
        baseColor: arrowMutedColor,
        baseOpacity: 0.85,
      };
      group.add(sprite);
    }
  }

  return group;
}

// ---------------------------------------------------------------------------
// Scene and camera
// ---------------------------------------------------------------------------

export function createViewCubeScene(cubeGroup: THREE.Group): THREE.Scene {
  const scene = new THREE.Scene();

  const ambient = new THREE.AmbientLight(0xffffff, 0.6);
  scene.add(ambient);

  const key = new THREE.DirectionalLight(0xffffff, 0.7);
  key.position.set(1, 1.5, 1.2);
  scene.add(key);

  const fill = new THREE.DirectionalLight(0xffffff, 0.3);
  fill.position.set(-0.8, 0.3, -0.6);
  scene.add(fill);

  scene.add(cubeGroup);
  return scene;
}

export function createViewCubeCamera(): THREE.OrthographicCamera {
  // Wide enough for the ring of controls around the cube: at 1.75 the cube's own
  // silhouette reached 81 % of the half-width and the Home/Fit/orbit buttons had
  // nowhere to sit. At 2.2 the cube spans 45 % of the widget and the buttons get
  // a clean ring around it.
  const frustum = 2.2;
  const camera = new THREE.OrthographicCamera(
    -frustum / 2,
    frustum / 2,
    frustum / 2,
    -frustum / 2,
    0.05,
    10,
  );
  camera.up.set(0, 0, 1);
  camera.position.set(1.4, 0.8, 1.4);
  camera.lookAt(0, 0, 0);
  return camera;
}

// ---------------------------------------------------------------------------
// Viewport rect
// ---------------------------------------------------------------------------

export function getCubeViewportRect(
  canvasWidth: number,
  canvasHeight: number,
  dpr: number,
): { x: number; y: number; width: number; height: number } {
  const size = CUBE_VIEWPORT_SIZE * dpr;
  const margin = CUBE_VIEWPORT_MARGIN * dpr;
  return {
    x: canvasWidth - size - margin,
    y: canvasHeight - size - margin,
    width: size,
    height: size,
  };
}

// ---------------------------------------------------------------------------
// Pointer-area check
// ---------------------------------------------------------------------------

export function isPointerInCubeArea(
  event: PointerEvent,
  canvasRect: DOMRect,
  dpr: number,
): boolean {
  const size = CUBE_VIEWPORT_SIZE * dpr;
  const margin = CUBE_VIEWPORT_MARGIN * dpr;
  const canvasRight = (canvasRect.right - canvasRect.left) * dpr;
  const px = (event.clientX - canvasRect.left) * dpr;
  const py = (event.clientY - canvasRect.top) * dpr;
  return (
    px >= canvasRight - size - margin &&
    px <= canvasRight - margin &&
    py >= margin &&
    py <= margin + size
  );
}

export function setViewCubeRaycasterFromPointer(
  raycaster: THREE.Raycaster,
  cubeCamera: THREE.Camera,
  // Structural so the same helper serves pointer events and the contextmenu
  // event (a MouseEvent) that opens the cube's menu.
  event: { clientX: number; clientY: number },
  canvas: HTMLCanvasElement,
  dpr: number,
) {
  const canvasRect = canvas.getBoundingClientRect();
  const rect = getCubeViewportRect(canvas.width, canvas.height, dpr);
  const glX = (event.clientX - canvasRect.left) * dpr;
  const glY = canvas.height - (event.clientY - canvasRect.top) * dpr;
  const ndcX = ((glX - rect.x) / rect.width) * 2 - 1;
  const ndcY = ((glY - rect.y) / rect.height) * 2 - 1;
  raycaster.setFromCamera(new THREE.Vector2(ndcX, ndcY), cubeCamera);
}

// ---------------------------------------------------------------------------
// Sync cube camera to match main view direction
// ---------------------------------------------------------------------------

export function syncCubeCamera(
  mainCamera: THREE.Camera,
  controlsTarget: THREE.Vector3,
  cubeCamera: THREE.OrthographicCamera,
): void {
  const dir = new THREE.Vector3()
    .copy(mainCamera.position)
    .sub(controlsTarget)
    .normalize();

  cubeCamera.position.copy(dir.clone().multiplyScalar(2));

  // Sync the cube camera's up to the main camera's up so the cube
  // renders with the same orientation the user sees on the model.
  // A fixed Z-up makes the cube faces misalign with the model when
  // the main camera's up has been tilted by prior view transitions.
  const upDotView = Math.abs(dir.dot(mainCamera.up));
  if (upDotView > 0.99) {
    // Main camera up is nearly parallel to the view direction —
    // a degenerate state.  Fall back to a world reference.
    const zUp = new THREE.Vector3(0, 0, 1);
    cubeCamera.up.copy(
      Math.abs(dir.dot(zUp)) > 0.99
        ? new THREE.Vector3(0, 1, 0)
        : zUp,
    );
  } else {
    cubeCamera.up.copy(mainCamera.up);
  }

  cubeCamera.lookAt(0, 0, 0);
}

export function updateSketchRotationArrows(
  cubeGroup: THREE.Group,
  cubeCamera: THREE.OrthographicCamera,
  visible: boolean,
): void {
  const right = new THREE.Vector3();
  const up = new THREE.Vector3();
  cubeCamera.updateMatrixWorld();
  right.setFromMatrixColumn(cubeCamera.matrixWorld, 0).normalize();
  up.setFromMatrixColumn(cubeCamera.matrixWorld, 1).normalize();

  cubeGroup.traverse((child) => {
    if (child.userData.cubeType === "button") {
      // Home top-left, Fit bottom-right, Backside bottom-left: FreeCAD's
      // arrangement. Placed in camera space so they never rotate with the cube.
      const action = child.userData.action as CubeButton;
      const horizontal =
        action === "fit" || action === "menu" ? 0.82 : -0.82;
      const vertical =
        action === "home" || action === "menu" ? 0.82 : -0.82;
      child.visible = true;
      child.position
        .copy(right)
        .multiplyScalar(horizontal)
        .add(up.clone().multiplyScalar(vertical));
      return;
    }
    if (child.userData.cubeType === "orbit_arrow") {
      // North above, south below, east right, west left. The glyph points the
      // way the camera moves, so north is direction -1 about the right axis.
      const axis = child.userData.axis as "up" | "right";
      const direction = child.userData.direction as -1 | 1;
      const horizontal = axis === "up" ? direction * 0.82 : 0;
      const vertical = axis === "right" ? -direction * 0.82 : 0;
      child.visible = true;
      child.position
        .copy(right)
        .multiplyScalar(horizontal)
        .add(up.clone().multiplyScalar(vertical));
      return;
    }
    if (child.userData.cubeType !== "rotation_arrow") {
      return;
    }
    child.visible = visible;
    if (!visible) {
      return;
    }
    const direction = child.userData.direction as -1 | 1;
    child.position
      .copy(up)
      .multiplyScalar(0.58)
      .add(right.clone().multiplyScalar(direction * 0.48));
  });
}

// ---------------------------------------------------------------------------
// Raycast against cube meshes
// ---------------------------------------------------------------------------

export function raycastViewCube(
  raycaster: THREE.Raycaster,
  cubeGroup: THREE.Group,
): ViewCubeHit {
  const targets: THREE.Object3D[] = [];
  cubeGroup.traverse((child) => {
    if (
      ((child as THREE.Mesh).isMesh || (child as THREE.Sprite).isSprite) &&
      child.userData.cubePart &&
      child.visible
    ) {
      targets.push(child);
    }
  });

  const intersections = raycaster.intersectObjects(targets, false);
  if (intersections.length === 0) return null;

  // Pick closest; faces (renderOrder 0) < edges (1) < corners (2)
  const hit = intersections[0].object as THREE.Mesh;
  const ud = hit.userData;

  if (ud.cubeType === "face") {
    return { type: "face", face: ud.face as CubeFace };
  }
  if (ud.cubeType === "edge") {
    return {
      type: "edge",
      face1: ud.face1 as CubeFace,
      face2: ud.face2 as CubeFace,
    };
  }
  if (ud.cubeType === "corner") {
    return {
      type: "corner",
      faces: ud.faces as [CubeFace, CubeFace, CubeFace],
    };
  }
  if (ud.cubeType === "rotation_arrow") {
    return { type: "rotation_arrow", direction: ud.direction as -1 | 1 };
  }
  if (ud.cubeType === "orbit_arrow") {
    return {
      type: "orbit_arrow",
      axis: ud.axis as "up" | "right",
      direction: ud.direction as -1 | 1,
    };
  }
  if (ud.cubeType === "button") {
    return { type: "button", action: ud.action as CubeButton };
  }
  return null;
}

// ---------------------------------------------------------------------------
// Hit → world direction for snap
// ---------------------------------------------------------------------------

export function getCubeHitTargetDirection(
  hit: Extract<
    NonNullable<ViewCubeHit>,
    { type: "face" | "edge" | "corner" }
  >,
): THREE.Vector3 {
  if (hit.type === "face") {
    return FACE_NORMALS[hit.face].clone();
  }
  if (hit.type === "edge") {
    return FACE_NORMALS[hit.face1]
      .clone()
      .add(FACE_NORMALS[hit.face2])
      .normalize();
  }
  // corner
  const dir = new THREE.Vector3();
  for (const f of hit.faces) {
    dir.add(FACE_NORMALS[f]);
  }
  return dir.normalize();
}

export function isCardinalCubeDirection(direction: THREE.Vector3): boolean {
  const normalized = direction.clone().normalize();
  return Object.values(FACE_NORMALS).some(
    (normal) => Math.abs(normalized.dot(normal)) > 0.985,
  );
}

function getDefaultUpForDirection(direction: THREE.Vector3): THREE.Vector3 {
  const worldUp = new THREE.Vector3(0, 0, 1);
  if (Math.abs(direction.dot(worldUp)) > 0.95) {
    return new THREE.Vector3(0, 1, 0);
  }
  return worldUp;
}

export function getQuantizedCubeUp(
  targetDirection: THREE.Vector3,
  currentUp: THREE.Vector3,
): THREE.Vector3 {
  const axis = targetDirection.clone().normalize();
  const defaultUp = getDefaultUpForDirection(axis);
  const baseUp = defaultUp
    .clone()
    .addScaledVector(axis, -defaultUp.dot(axis))
    .normalize();
  const projectedCurrentUp = currentUp
    .clone()
    .addScaledVector(axis, -currentUp.dot(axis));

  if (projectedCurrentUp.lengthSq() < 1e-6) {
    return baseUp;
  }

  projectedCurrentUp.normalize();
  // Orientations are counted in the facet's own symmetry, never in diagonals.
  //
  // A corner is a three-face direction and its chamfer facet is a triangle:
  // sixfold (60°) around the view axis, as FreeCAD's navigator does. Faces and
  // edges are fourfold (90°), so a quarter turn keeps the view square — a 60°
  // step there would leave the model rotated by 30°.
  //
  // This used 45° steps (8 candidates) for everything, to keep the quantized up
  // close to the current one. The cost was that a corner snap stored a DIAGONAL
  // camera up and every later face click inherited it: clicking TOP produced a
  // top view rolled 45°, with no way back to a straight view (reported
  // 2026-11-19, "I get stuck in between axes at 45 degrees"). Never quantize to
  // a diagonal.
  const isCorner =
    [Math.abs(axis.x), Math.abs(axis.y), Math.abs(axis.z)].filter(
      (component) => component > 0.3,
    ).length === 3;
  const steps = isCorner ? 6 : 4;
  const stepAngle = (Math.PI * 2) / steps;

  let bestUp = baseUp.clone();
  let bestDot = -Infinity;
  for (let step = 0; step < steps; step += 1) {
    const candidate = baseUp
      .clone()
      .applyAxisAngle(axis, step * stepAngle)
      .normalize();
    const dot = candidate.dot(projectedCurrentUp);
    if (dot > bestDot) {
      bestDot = dot;
      bestUp = candidate;
    }
  }
  return bestUp;
}

// ---------------------------------------------------------------------------
// Camera animation
// ---------------------------------------------------------------------------

export function animateCameraTowardTarget(
  camera: THREE.Camera,
  controls: { target: THREE.Vector3; update: () => void },
  startPosition: THREE.Vector3,
  targetPosition: THREE.Vector3,
  startTime: number,
  currentTime: number,
  startUp?: THREE.Vector3,
  targetUp?: THREE.Vector3,
): boolean {
  const elapsed = currentTime - startTime;
  const t = Math.min(elapsed / ANIMATION_DURATION, 1);
  // cubic ease-out matching the design system cue
  const ease = 1 - Math.pow(1 - t, 3);

  camera.position.lerpVectors(startPosition, targetPosition, ease);
  if (startUp && targetUp) {
    // Spherical-linear interpolate the up so it follows a consistent
    // arc instead of passing through degenerate intermediate values.
    // Linear nlerp can cross through a direction parallel to the view
    // direction, causing the sketch axes to flip momentarily.
    const upCross = new THREE.Vector3()
      .crossVectors(startUp, targetUp);
    const upCrossLen = upCross.length();
    if (upCrossLen > 1e-6) {
      const upAngle = Math.acos(
        THREE.MathUtils.clamp(startUp.dot(targetUp), -1, 1),
      );
      camera.up
        .copy(startUp)
        .applyAxisAngle(
          upCross.divideScalar(upCrossLen),
          upAngle * ease,
        )
        .normalize();
    } else {
      camera.up.lerpVectors(startUp, targetUp, ease).normalize();
    }
  }
  camera.lookAt(controls.target);
  controls.update();

  return t >= 1;
}

// ---------------------------------------------------------------------------
// Hover visual state
// ---------------------------------------------------------------------------

function cubeMaterial(
  child: THREE.Object3D,
): THREE.MeshBasicMaterial | THREE.SpriteMaterial | null {
  if (
    !((child as THREE.Mesh).isMesh || (child as THREE.Sprite).isSprite) ||
    !child.userData.cubePart
  ) {
    return null;
  }
  return (child as THREE.Mesh | THREE.Sprite).material as
    | THREE.MeshBasicMaterial
    | THREE.SpriteMaterial;
}

export function applyCubeHover(cubeGroup: THREE.Group, hit: ViewCubeHit): void {
  clearCubeHover(cubeGroup);

  if (!hit) return;

  // The cursor is on the widget: bring it to full strength, and paint the picked
  // facet in the highlight colour.
  cubeGroup.traverse((child) => {
    const mat = cubeMaterial(child);
    if (mat) {
      mat.opacity = 1;
    }
  });

  let target: THREE.Object3D | null = null;

  cubeGroup.traverse((child) => {
    if (!(child as THREE.Mesh).isMesh || !child.userData.cubePart) return;
    const ud = child.userData;
    if (hit.type === "face" && ud.cubeType === "face" && ud.face === hit.face) {
      target = child;
    } else if (
      hit.type === "edge" &&
      ud.cubeType === "edge" &&
      ((ud.face1 === hit.face1 && ud.face2 === hit.face2) ||
        (ud.face1 === hit.face2 && ud.face2 === hit.face1))
    ) {
      target = child;
    } else if (
      hit.type === "corner" &&
      ud.cubeType === "corner" &&
      ud.faces &&
      ud.faces.length === 3 &&
      hit.faces.every((f: CubeFace) => (ud.faces as CubeFace[]).includes(f))
    ) {
      target = child;
    } else if (
      hit.type === "rotation_arrow" &&
      ud.cubeType === "rotation_arrow" &&
      ud.direction === hit.direction
    ) {
      target = child;
    } else if (
      hit.type === "orbit_arrow" &&
      ud.cubeType === "orbit_arrow" &&
      ud.axis === hit.axis &&
      ud.direction === hit.direction
    ) {
      target = child;
    } else if (
      hit.type === "button" &&
      ud.cubeType === "button" &&
      ud.action === hit.action
    ) {
      target = child;
    }
  });

  if (target) {
    const mat = (target as THREE.Mesh | THREE.Sprite).material as
      | THREE.MeshBasicMaterial
      | THREE.SpriteMaterial;
    mat.color.set(viewCubeColor("--cad-viewcube-hover", "#00e5ff"));
  }
}

export function clearCubeHover(cubeGroup: THREE.Group): void {
  cubeGroup.traverse((child) => {
    const mat = cubeMaterial(child);
    if (!mat) {
      return;
    }
    mat.color.set(child.userData.baseColor as string);
    // Idle again: half strength, as FreeCAD's navigator does when the cursor
    // leaves it. Sprites keep the opacity they were built with (the roll arrows
    // are already shown only when they apply, the buttons are always on).
    mat.opacity =
      (child.userData.baseOpacity as number | undefined) ?? INACTIVE_OPACITY;
  });
}

/**
 * Orbit the camera by whole quarter turns about one of its screen axes.
 *
 * The cube's arrows are camera-centric: the arrow points the way the camera
 * moves (FreeCAD's ArrowNorth/South/East/West). "up" is the vertical axis, so
 * east/west swing around the model; "right" is the horizontal one, so
 * north/south lift the eye over the top or drop it underneath. The pivot and the
 * eye distance are kept; `lookAt` then re-derives the orientation, and the
 * clamp keeps the eye off the pole where the up vector would roll.
 */
export function orbitCameraQuarterTurns(
  camera: THREE.Camera,
  controls: { target: THREE.Vector3; update?: () => void },
  axis: "up" | "right",
  turns: number,
): void {
  const up = camera.up.clone().normalize();
  if (up.lengthSq() < 0.5) {
    up.set(0, 0, 1);
  }
  const offset = new THREE.Vector3().copy(camera.position).sub(controls.target);
  if (offset.lengthSq() < 1e-12) {
    return;
  }

  let rotationAxis: THREE.Vector3;
  if (axis === "up") {
    rotationAxis = up;
  } else {
    // Screen-right in the eye's frame. Z-up: right = up x view, view = -offset.
    rotationAxis = new THREE.Vector3().crossVectors(up, offset).normalize();
    if (rotationAxis.lengthSq() < 0.5) {
      return;
    }
  }

  offset.applyAxisAngle(rotationAxis, turns * (Math.PI / 2));

  if (axis === "right") {
    // Never let a north/south turn carry the eye through the pole.
    const elevation = Math.asin(
      THREE.MathUtils.clamp(offset.clone().normalize().dot(up), -1, 1),
    );
    const limit = Math.PI / 2 - POLE_EPSILON;
    if (Math.abs(elevation) > limit) {
      const corrected = handlePoleClamp(offset, up, limit, elevation);
      offset.copy(corrected);
    }
  }

  camera.position.copy(controls.target).add(offset);
  camera.lookAt(controls.target);
  controls.update?.();
}

function handlePoleClamp(
  offset: THREE.Vector3,
  up: THREE.Vector3,
  limit: number,
  elevation: number,
): THREE.Vector3 {
  // Rotate the offset back towards the horizon by the excess angle.
  const axis = new THREE.Vector3().crossVectors(offset, up);
  if (axis.lengthSq() < 1e-12) {
    return offset;
  }
  const excess = Math.abs(elevation) - limit;
  return offset
    .clone()
    .applyAxisAngle(axis.normalize(), (elevation > 0 ? -1 : 1) * excess);
}

// ---------------------------------------------------------------------------
// Drag-to-orbit: apply pointer delta as spherical rotation
// ---------------------------------------------------------------------------

const WORLD_Y = new THREE.Vector3(0, 1, 0);
// How close to a pole the view may get before `lookAt` starts rolling the
// picture. SolidCraft clamps pitch to +-(90 - 1e-4) degrees
// (crates/render/src/camera.rs, Camera::orbit) so an orbit can never cross the
// up axis upside down; 0.01 rad is the same limit with room to spare.
const POLE_EPSILON = 0.01;

/**
 * Orbit the camera by a pointer delta, about the camera's OWN up axis.
 *
 * `THREE.Spherical` measures theta about +Y and phi from +Y. Our world is Z-up,
 * so using it directly rotated the model about a HORIZONTAL axis: dragging the
 * cube right tipped the board sideways instead of turning it, and dragging down
 * slid the camera north instead of lifting it. Reported 2026-11-19 as "I drag to
 * the right and the body starts rotating to the left".
 *
 * The fix is the quaternion pair three's OrbitControls itself uses: map the
 * offset into a frame where `camera.up` is +Y, do the spherical delta there,
 * map back. The cube drag therefore cannot disagree with dragging the
 * background. SolidCraft goes one step further and routes its cube through the
 * very same orbit call as the background (crates/ui-egui/src/navigation_cube.rs
 * -> orbit_drag -> Camera::orbit), which is the structural version of this fix.
 *
 * Signs follow their convention: the model follows the pointer (drag right turns
 * the side facing you to the right), and dragging down lifts the eye towards the
 * top view.
 */
export function applyCubeDragOrbit(
  camera: THREE.Camera,
  controls: { target: THREE.Vector3; update: () => void },
  deltaX: number,
  deltaY: number,
  sensitivity: number,
): void {
  const up = camera.up.clone().normalize();
  if (up.lengthSq() < 0.5) {
    up.set(0, 0, 1);
  }
  const offset = new THREE.Vector3().copy(camera.position).sub(controls.target);
  const toUpFrame = new THREE.Quaternion().setFromUnitVectors(up, WORLD_Y);
  const fromUpFrame = toUpFrame.clone().invert();
  offset.applyQuaternion(toUpFrame);

  const spherical = new THREE.Spherical().setFromVector3(offset);
  spherical.theta -= deltaX * sensitivity;
  spherical.phi -= deltaY * sensitivity;
  spherical.phi = Math.max(
    POLE_EPSILON,
    Math.min(Math.PI - POLE_EPSILON, spherical.phi),
  );

  offset.setFromSpherical(spherical).applyQuaternion(fromUpFrame);
  camera.position.copy(controls.target).add(offset);
  camera.lookAt(controls.target);
  controls.update();
}

// ---------------------------------------------------------------------------
// Dispose helper
// ---------------------------------------------------------------------------

export function disposeViewCubeGroup(group: THREE.Group): void {
  group.traverse((child) => {
    if (!((child as THREE.Mesh).isMesh || (child as THREE.Sprite).isSprite))
      return;
    const object = child as THREE.Mesh | THREE.Sprite;
    if ((object as THREE.Mesh).geometry) {
      (object as THREE.Mesh).geometry.dispose();
    }
    const mat = object.material as
      | THREE.MeshBasicMaterial
      | THREE.SpriteMaterial;
    if (Array.isArray(mat)) {
      for (const m of mat) {
        const basicMat = m as THREE.MeshBasicMaterial;
        if (basicMat.map) basicMat.map.dispose();
        basicMat.dispose();
      }
    } else {
      if (mat.map) mat.map.dispose();
      mat.dispose();
    }
  });
  group.clear();
}

// ---------------------------------------------------------------------------
// Render-target-based cube rendering (avoids scissor-test GPU dependency)
// ---------------------------------------------------------------------------
// On some Windows ANGLE backends the scissor test behaves differently or
// fails silently. Instead, render the cube to an offscreen target and blit
// the result to the corner with a textured NDC-space plane — no scissor.

export function createCubeRenderTarget(
  renderer: THREE.WebGLRenderer,
): THREE.WebGLRenderTarget {
  const dpr = renderer.getPixelRatio();
  const size = Math.round(CUBE_VIEWPORT_SIZE * dpr);
  return new THREE.WebGLRenderTarget(size, size, {
    minFilter: THREE.LinearFilter,
    magFilter: THREE.LinearFilter,
    format: THREE.RGBAFormat,
  });
}

export function resizeCubeRenderTarget(
  target: THREE.WebGLRenderTarget,
  dpr: number,
): void {
  const size = Math.round(CUBE_VIEWPORT_SIZE * dpr);
  if (target.width !== size || target.height !== size) {
    target.setSize(size, size);
  }
}

export interface CubeBlitScene {
  scene: THREE.Scene;
  camera: THREE.OrthographicCamera;
  mesh: THREE.Mesh;
}

export function createCubeBlitScene(
  renderTarget: THREE.WebGLRenderTarget,
): CubeBlitScene {
  const scene = new THREE.Scene();
  const camera = new THREE.OrthographicCamera(-1, 1, 1, -1, 0, 1);

  const geometry = new THREE.PlaneGeometry(1, 1);
  const material = new THREE.MeshBasicMaterial({
    map: renderTarget.texture,
    transparent: true,
    depthTest: false,
    depthWrite: false,
  });
  const mesh = new THREE.Mesh(geometry, material);
  mesh.renderOrder = 999;
  scene.add(mesh);

  return { scene, camera, mesh };
}

export function updateCubeBlitMesh(
  mesh: THREE.Mesh,
  canvasWidth: number,
  canvasHeight: number,
): void {
  const sizeCSS = CUBE_VIEWPORT_SIZE;
  const marginCSS = CUBE_VIEWPORT_MARGIN;

  // Non-uniform NDC scale so the plane renders as a square in CSS pixels.
  const ndcScaleX = (2 * sizeCSS) / canvasWidth;
  const ndcScaleY = (2 * sizeCSS) / canvasHeight;
  mesh.scale.set(ndcScaleX, ndcScaleY, 1);

  // Center position in NDC: top-right corner, inset by margin.
  mesh.position.set(
    1 - (2 * (marginCSS + sizeCSS / 2)) / canvasWidth,
    1 - (2 * (marginCSS + sizeCSS / 2)) / canvasHeight,
    0,
  );
}

export function disposeCubeBlitScene(blit: CubeBlitScene): void {
  (blit.mesh.material as THREE.MeshBasicMaterial).dispose();
  blit.mesh.geometry.dispose();
}
