import type {
  PlaneFrame,
  SolidFacePlaneFrame,
  ViewportSolidFace,
} from "../types";

type AsyncVoid = () => Promise<void>;
type RunAction = (action: AsyncVoid) => Promise<void>;

export function toCorePlaneFrame(planeFrame: SolidFacePlaneFrame): PlaneFrame {
  return {
    origin: {
      x: planeFrame.origin[0],
      y: planeFrame.origin[1],
      z: planeFrame.origin[2],
    },
    x_axis: {
      x: planeFrame.xAxis[0],
      y: planeFrame.xAxis[1],
      z: planeFrame.xAxis[2],
    },
    y_axis: {
      x: planeFrame.yAxis[0],
      y: planeFrame.yAxis[1],
      z: planeFrame.yAxis[2],
    },
    normal: {
      x: planeFrame.normal[0],
      y: planeFrame.normal[1],
      z: planeFrame.normal[2],
    },
  };
}

export async function startSketchOnSelectedPlaneOrFace({
  activeSketchPlaneId,
  selectedReferenceId,
  selectedSketchableFace,
  runAction,
  startSketchOnPlane,
  startSketchOnFace,
}: {
  activeSketchPlaneId: string | null;
  selectedReferenceId: string | null;
  selectedSketchableFace: ViewportSolidFace | null;
  runAction: RunAction;
  startSketchOnPlane: (referenceId: string) => Promise<void>;
  startSketchOnFace: (
    faceId: string,
    planeFrame: PlaneFrame,
  ) => Promise<void>;
}) {
  if (activeSketchPlaneId) {
    return;
  }

  if (selectedReferenceId) {
    await runAction(async () => {
      await startSketchOnPlane(selectedReferenceId);
    });
    return;
  }

  if (selectedSketchableFace) {
    await runAction(async () => {
      await startSketchOnFace(
        selectedSketchableFace.face_id,
        selectedSketchableFace.plane_frame,
      );
    });
  }
}
