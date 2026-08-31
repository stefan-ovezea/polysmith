import * as THREE from "three";

import type { DocumentState, StockDefinition, ViewportState } from "@/types";
import { themeColor } from "@/utils";

export function addCamSceneObjects({
  document,
  viewport,
  referenceGroup,
  showStock,
  wcsOrientation,
}: {
  document: DocumentState | null;
  viewport: ViewportState | null;
  referenceGroup: THREE.Group;
  showStock: boolean;
  wcsOrientation: string;
}) {
  const setup = document?.cam?.setups?.[0];
  if (!setup) {
    return;
  }

  // The origin marker sits at the STOCK origin — the machine zero the
  // user edits in the setup panel (falls back to the model center).
  const stockOrigin = setup.stock?.origin;
  const origin: [number, number, number] = stockOrigin
    ? stockOrigin
    : (() => {
        const center = modelCenterFromBodies(viewport?.bodies ?? []);
        return [center.x, center.y, center.z];
      })();

  addWcsOriginMarker({
    origin,
    referenceGroup,
  });

  if (showStock && setup.stock) {
    addStockBoundingBox({
      stock: setup.stock,
      viewport,
      referenceGroup,
    });
  }
}

export function addCamToolpathLines({
  viewport,
  contentGroup,
}: {
  viewport: ViewportState | null;
  contentGroup: THREE.Group;
}) {
  // One THREE.Line per segment was a draw call per segment — merge
  // into two batched LineSegments per toolpath (rapid vs feed), plus
  // pierce-point dots.
  const toolpathLines: THREE.Object3D[] = [];
  const rapidColor = themeColor("--cad-toolpath-rapid", "#ff4444");
  const feedColor = themeColor("--cad-toolpath-feed", "#44ff44");

  for (const toolpath of viewport?.toolpaths ?? []) {
    const rapidPositions: number[] = [];
    const feedPositions: number[] = [];
    for (let index = 1; index < toolpath.points.length; index += 1) {
      const previous = toolpath.points[index - 1];
      const current = toolpath.points[index];
      const positions = current.is_rapid ? rapidPositions : feedPositions;
      positions.push(
        previous.x,
        previous.y,
        previous.z,
        current.x,
        current.y,
        current.z,
      );
    }

    const makeSegments = (
      positions: number[],
      color: string,
      opacity: number,
      isRapid: boolean,
    ) => {
      if (positions.length === 0) {
        return null;
      }
      const geometry = new THREE.BufferGeometry();
      geometry.setAttribute(
        "position",
        new THREE.Float32BufferAttribute(positions, 3),
      );
      const segments = new THREE.LineSegments(
        geometry,
        new THREE.LineBasicMaterial({
          color,
          transparent: true,
          opacity,
          depthTest: false,
          depthWrite: false,
        }),
      );
      segments.renderOrder = 10;
      segments.userData.toolpathId = toolpath.id;
      segments.userData.isRapid = isRapid;
      contentGroup.add(segments);
      return segments;
    };

    const rapid = makeSegments(rapidPositions, rapidColor, 0.5, true);
    if (rapid) {
      toolpathLines.push(rapid);
    }
    const feed = makeSegments(feedPositions, feedColor, 0.85, false);
    if (feed) {
      toolpathLines.push(feed);
    }

    // Pierce markers: small dots where the beam dwells before cutting.
    const piercePositions: number[] = [];
    for (const point of toolpath.points) {
      if (point.pierce) {
        piercePositions.push(point.x, point.y, point.z);
      }
    }
    if (piercePositions.length > 0) {
      const dotsGeometry = new THREE.BufferGeometry();
      dotsGeometry.setAttribute(
        "position",
        new THREE.Float32BufferAttribute(piercePositions, 3),
      );
      const dots = new THREE.Points(
        dotsGeometry,
        new THREE.PointsMaterial({
          color: rapidColor,
          size: 3.5,
          sizeAttenuation: false,
          transparent: true,
          opacity: 0.9,
          depthTest: false,
          depthWrite: false,
        }),
      );
      dots.renderOrder = 11;
      dots.userData.toolpathId = toolpath.id;
      contentGroup.add(dots);
      toolpathLines.push(dots);
    }
  }

  return toolpathLines;
}

// The machining origin marker: a triaxis gizmo (sphere + arrowheads)
// at the stock origin.  Axis colors come from theme tokens.
function addWcsOriginMarker({
  origin,
  referenceGroup,
}: {
  origin: [number, number, number];
  referenceGroup: THREE.Group;
}) {
  const originPoint = new THREE.Vector3(origin[0], origin[1], origin[2]);
  const axisLen = 50;

  const makeAxis = (dir: THREE.Vector3, color: string) => {
    const end = originPoint.clone().add(dir.clone().multiplyScalar(axisLen));
    const lineGeometry = new THREE.BufferGeometry().setFromPoints([
      originPoint,
      end,
    ]);
    const line = new THREE.Line(
      lineGeometry,
      new THREE.LineBasicMaterial({
        color,
        depthTest: false,
        depthWrite: false,
        transparent: true,
        opacity: 0.95,
      }),
    );
    line.renderOrder = 20;
    referenceGroup.add(line);

    // Arrowhead cone pointing along the axis.
    const cone = new THREE.Mesh(
      new THREE.ConeGeometry(4, 12, 12),
      new THREE.MeshBasicMaterial({
        color,
        depthTest: false,
        depthWrite: false,
        transparent: true,
        opacity: 0.95,
      }),
    );
    const tip = originPoint
      .clone()
      .add(dir.clone().multiplyScalar(axisLen + 6));
    cone.position.copy(tip);
    const axis = new THREE.Vector3(0, 1, 0);
    cone.quaternion.setFromUnitVectors(axis, dir.clone().normalize());
    cone.renderOrder = 20;
    referenceGroup.add(cone);
  };

  const sphere = new THREE.Mesh(
    new THREE.SphereGeometry(3.5, 16, 16),
    new THREE.MeshBasicMaterial({
      color: themeColor("--color-axis-x", "#ff6b7a"),
      depthTest: false,
      depthWrite: false,
      transparent: true,
      opacity: 0.95,
    }),
  );
  sphere.position.copy(originPoint);
  sphere.renderOrder = 20;
  referenceGroup.add(sphere);

  makeAxis(
    new THREE.Vector3(1, 0, 0),
    themeColor("--color-axis-x", "#ff6b7a"),
  );
  makeAxis(
    new THREE.Vector3(0, 1, 0),
    themeColor("--color-axis-y", "#2bd978"),
  );
  makeAxis(
    new THREE.Vector3(0, 0, 1),
    themeColor("--color-axis-z", "#6db4ff"),
  );
}

function addStockBoundingBox({
  stock,
  viewport,
  referenceGroup,
}: {
  stock: StockDefinition;
  viewport: ViewportState | null;
  referenceGroup: THREE.Group;
}) {
  const modelCenter = modelCenterFromBodies(viewport?.bodies ?? []);
  // The stock always WRAPS THE PART — it is centered on the model
  // bounds, never on the picked origin.  The origin (WCS zero) is an
  // arbitrary point relative to the stock: picking a part corner must
  // move the origin marker, NOT the stock box.
  const stockCenter = modelCenter;
  const margin = stock.margin ?? 3;

  let stockWidth: number;
  let stockHeight: number;
  let stockDepth: number;
  if (stock.type === "cylinder" && stock.diameter !== undefined) {
    // Cylinder stock approximated by its bounding box for display.
    const diameter = stock.diameter + margin * 2;
    stockWidth = diameter;
    stockHeight = diameter;
    stockDepth = (stock.length ?? 20) + margin * 2;
  } else {
    const size = stock.size ?? [120, 120, 20];
    stockWidth = size[0] + margin * 2;
    stockHeight = size[1] + margin * 2;
    stockDepth = size[2] + margin * 2;
  }

  const stockBox = new THREE.BoxGeometry(stockWidth, stockHeight, stockDepth);
  const stockColor = themeColor("--color-axis-z", "#4488ff");

  const stockMesh = new THREE.Mesh(
    stockBox,
    new THREE.MeshBasicMaterial({
      color: stockColor,
      transparent: true,
      opacity: 0.15,
      depthTest: true,
      depthWrite: false,
    }),
  );
  stockMesh.position.copy(stockCenter);
  stockMesh.renderOrder = 0;
  referenceGroup.add(stockMesh);

  const stockEdges = new THREE.LineSegments(
    new THREE.EdgesGeometry(stockBox),
    new THREE.LineBasicMaterial({
      color: stockColor,
      transparent: true,
      opacity: 0.35,
      depthTest: true,
      depthWrite: false,
    }),
  );
  stockEdges.position.copy(stockCenter);
  stockEdges.renderOrder = 2;
  referenceGroup.add(stockEdges);
}

function modelCenterFromBodies(bodies: ViewportState["bodies"]) {
  let minX = Infinity;
  let minY = Infinity;
  let minZ = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  let maxZ = -Infinity;

  for (const body of bodies) {
    const halfWidth = body.size.x / 2;
    const halfHeight = body.size.y / 2;
    const halfDepth = body.size.z / 2;
    minX = Math.min(minX, body.center.x - halfWidth);
    maxX = Math.max(maxX, body.center.x + halfWidth);
    minY = Math.min(minY, body.center.y - halfHeight);
    maxY = Math.max(maxY, body.center.y + halfHeight);
    minZ = Math.min(minZ, body.center.z - halfDepth);
    maxZ = Math.max(maxZ, body.center.z + halfDepth);
  }

  return new THREE.Vector3(
    Number.isFinite(minX) ? (minX + maxX) / 2 : 0,
    Number.isFinite(minY) ? (minY + maxY) / 2 : 0,
    Number.isFinite(minZ) ? (minZ + maxZ) / 2 : 0,
  );
}
