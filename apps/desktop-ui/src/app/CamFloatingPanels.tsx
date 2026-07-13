import type { CamOperationUpdatePayload, CamSetupUpdatePayload } from "../lib";
import type { DocumentState, ViewportState } from "../types";
import { CamSetupPanel, FaceMillingPanel } from "../layout";
import type { ToolOption } from "../layout/FaceMillingPanel";

type RunAction = (action: () => Promise<void>) => Promise<void>;

interface CamFloatingPanelsProps {
  document: DocumentState | null;
  viewport: ViewportState | null;
  disabled: boolean;
  isSetupPanelOpen: boolean;
  selectedOperationId: string | null;
  showStock: boolean;
  wcsOrientation: string;
  setShowStock: (show: boolean) => void;
  setWcsOrientation: (mode: string) => void;
  setSetupPanelOpen: (open: boolean) => void;
  setSelectedOperationId: (operationId: string | null) => void;
  runAction: RunAction;
  camSetupUpdate: (payload: CamSetupUpdatePayload) => Promise<void>;
  camOperationUpdate: (payload: CamOperationUpdatePayload) => Promise<void>;
  camOperationDelete: (operationId: string) => Promise<void>;
}

export function CamFloatingPanels({
  document,
  viewport,
  disabled,
  isSetupPanelOpen,
  selectedOperationId,
  showStock,
  wcsOrientation,
  setShowStock,
  setWcsOrientation,
  setSetupPanelOpen,
  setSelectedOperationId,
  runAction,
  camSetupUpdate,
  camOperationUpdate,
  camOperationDelete,
}: CamFloatingPanelsProps) {
  const setupPanel = isSetupPanelOpen ? (
    <CamSetupPanel
      initialSetup={{
        stock: (document?.cam as any)?.setups?.[0]?.stock ?? {
          width: 120,
          height: 120,
          depth: 20,
          offset_x: 5,
          offset_y: 5,
          offset_z: 5,
        },
        wcs_origin: (document?.cam as any)?.setups?.[0]?.wcs_origin ?? { x: 0, y: 0, z: 0 },
        safety_plane_z: (document?.cam as any)?.setups?.[0]?.safety_plane_z ?? 10,
        wcs_angle: (document?.cam as any)?.setups?.[0]?.wcs_angle ?? 0,
        orientation_mode: "model",
        origin_mode: "model",
      }}
      bodies={(viewport?.bodies ?? []).map((body) => ({
        id: body.id,
        label: body.label,
        center: body.center,
        size: body.size,
      }))}
      showStock={showStock}
      onShowStockChange={setShowStock}
      wcsOrientation={wcsOrientation}
      onWcsOrientationChange={setWcsOrientation}
      disabled={disabled}
      onUpdate={(state) => {
        void runAction(async () => {
          await camSetupUpdate({
            stock: state.stock,
            wcs_origin: state.wcs_origin,
            safety_plane_z: state.safety_plane_z,
            wcs_angle: state.wcs_angle,
          });
        });
      }}
      onConfirm={() => setSetupPanelOpen(false)}
      onCancel={() => setSetupPanelOpen(false)}
    />
  ) : null;

  const millingPanel = selectedOperationId
    ? buildFaceMillingPanel({
        document,
        disabled,
        selectedOperationId,
        setSelectedOperationId,
        runAction,
        camOperationUpdate,
        camOperationDelete,
      })
    : null;

  return (
    <>
      {setupPanel}
      {millingPanel}
    </>
  );
}

function buildFaceMillingPanel({
  document,
  disabled,
  selectedOperationId,
  setSelectedOperationId,
  runAction,
  camOperationUpdate,
  camOperationDelete,
}: Pick<
  CamFloatingPanelsProps,
  | "document"
  | "disabled"
  | "selectedOperationId"
  | "setSelectedOperationId"
  | "runAction"
  | "camOperationUpdate"
  | "camOperationDelete"
>) {
  const ops = (document?.cam as any)?.operations;
  if (!ops || !selectedOperationId) {
    return null;
  }

  const operation = ops.find(
    (candidate: any) => candidate.id === selectedOperationId && candidate.type === 0,
  );
  if (!operation) {
    return null;
  }

  const tools: ToolOption[] = ((document?.cam as any)?.tool_library ?? []).map((tool: any) => ({
    tool_id: tool.tool_id,
    name: tool.name,
    diameter: tool.diameter,
  }));

  return (
    <FaceMillingPanel
      operationName={operation.name}
      initialParams={{
        depth: operation.face_milling?.depth ?? 0.5,
        stepover: operation.face_milling?.stepover ?? 5,
        angle_deg: operation.face_milling?.angle_deg ?? 0,
      }}
      initialToolId={operation.tool_id}
      tools={tools}
      disabled={disabled}
      onUpdate={(params, toolId) => {
        void runAction(async () => {
          await camOperationUpdate({
            operation_id: operation.id,
            tool_id: toolId,
            params,
          });
        });
      }}
      onDelete={() => {
        void runAction(async () => {
          await camOperationDelete(operation.id);
          setSelectedOperationId(null);
        });
      }}
      onConfirm={() => setSelectedOperationId(null)}
      onCancel={() => setSelectedOperationId(null)}
    />
  );
}
