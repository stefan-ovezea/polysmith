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
  const toolpathLines: THREE.Line[] = [];
  const rapidColor = themeColor("--cad-toolpath-rapid", "#ff4444");
  const feedColor = themeColor("--cad-toolpath-feed", "#44ff44");

  for (const toolpath of viewport?.toolpaths ?? []) {
    for (let index = 1; index < toolpath.points.length; index += 1) {
      const previous = toolpath.points[index - 1];
      const current = toolpath.points[index];
      const isRapid = current.is_rapid;
      const geometry = new THREE.BufferGeometry().setFromPoints([
        new THREE.Vector3(previous.x, previous.y, previous.z),
        new THREE.Vector3(current.x, current.y, current.z),
      ]);
      const material = new THREE.LineBasicMaterial({
        color: isRapid ? rapidColor : feedColor,
        transparent: true,
        opacity: isRapid ? 0.5 : 0.85,
        depthTest: false,
        depthWrite: false,
      });
      const line = new THREE.Line(geometry, material);
      line.renderOrder = 10;
      line.userData.toolpathId = toolpath.id;
      line.userData.isRapid = isRapid;
      toolpathLines.push(line);
      contentGroup.add(line);
    }
  }

  return toolpathLines;
}

// The WCS marker always renders in the machine Z-up orientation (the
// CAD model frame's X/Y/Z axes). Axis colors come from theme tokens.
function addWcsOriginMarker({
  origin,
  referenceGroup,
}: {
  origin: [number, number, number];
  referenceGroup: THREE.Group;
}) {
  const originPoint = new THREE.Vector3(origin[0], origin[1], origin[2]);
  const axisLen = 30;

  const makeAxis = (dir: THREE.Vector3, color: string) => {
    const end = originPoint.clone().add(dir.clone().multiplyScalar(axisLen));
    const geometry = new THREE.BufferGeometry().setFromPoints([
      originPoint,
      end,
    ]);
    const material = new THREE.LineBasicMaterial({
      color,
      linewidth: 1,
      depthTest: false,
      depthWrite: false,
      transparent: true,
      opacity: 0.9,
    });
    const line = new THREE.Line(geometry, material);
    line.renderOrder = 1;
    referenceGroup.add(line);
  };

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
  const stockCenter = stock.origin
    ? new THREE.Vector3(stock.origin[0], stock.origin[1], stock.origin[2])
    : modelCenter;
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
