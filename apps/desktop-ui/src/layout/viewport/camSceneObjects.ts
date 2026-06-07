import * as THREE from "three";

import type { DocumentState, ViewportState } from "@/types";

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
  if (!document?.cam_setup) {
    return;
  }

  addWcsOriginMarker({
    origin: document.cam_setup.wcs_origin,
    referenceGroup,
    wcsOrientation,
  });

  if (showStock && document.cam_setup.stock) {
    addStockBoundingBox({
      stock: document.cam_setup.stock,
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
        color: isRapid ? 0xff4444 : 0x44ff44,
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

function addWcsOriginMarker({
  origin,
  referenceGroup,
  wcsOrientation,
}: {
  origin: { x: number; y: number; z: number };
  referenceGroup: THREE.Group;
  wcsOrientation: string;
}) {
  const originPoint = new THREE.Vector3(origin.x, origin.y, origin.z);
  const axisLen = 20;

  const makeAxis = (dir: THREE.Vector3, color: number) => {
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

  const xAxis = new THREE.Vector3(1, 0, 0);
  const yAxis = new THREE.Vector3(0, 1, 0);
  const zAxis = new THREE.Vector3(0, 0, 1);

  if (wcsOrientation === "z_up") {
    yAxis.set(0, 0, -1);
    zAxis.set(0, 1, 0);
  } else if (wcsOrientation === "y_up") {
    zAxis.set(0, 0, -1);
  }

  makeAxis(xAxis, 0xff4444);
  makeAxis(yAxis, 0x44ff44);
  makeAxis(zAxis, 0x4488ff);
}

function addStockBoundingBox({
  stock,
  viewport,
  referenceGroup,
}: {
  stock: {
    width: number;
    height: number;
    depth: number;
    offset_x: number;
    offset_y: number;
    offset_z: number;
  };
  viewport: ViewportState | null;
  referenceGroup: THREE.Group;
}) {
  const stockWidth = stock.width + stock.offset_x * 2;
  const stockHeight = stock.height + stock.offset_y * 2;
  const stockDepth = stock.depth + stock.offset_z * 2;
  const modelCenter = modelCenterFromBodies(viewport?.bodies ?? []);
  const stockBox = new THREE.BoxGeometry(stockWidth, stockHeight, stockDepth);

  const stockMesh = new THREE.Mesh(
    stockBox,
    new THREE.MeshBasicMaterial({
      color: 0x4488ff,
      transparent: true,
      opacity: 0.15,
      depthTest: true,
      depthWrite: false,
    }),
  );
  stockMesh.position.copy(modelCenter);
  stockMesh.renderOrder = 0;
  referenceGroup.add(stockMesh);

  const stockEdges = new THREE.LineSegments(
    new THREE.EdgesGeometry(stockBox),
    new THREE.LineBasicMaterial({
      color: 0x4488ff,
      transparent: true,
      opacity: 0.35,
      depthTest: true,
      depthWrite: false,
    }),
  );
  stockEdges.position.copy(modelCenter);
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
