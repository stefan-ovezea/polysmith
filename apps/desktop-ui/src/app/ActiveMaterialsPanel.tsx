import { MaterialsPanel } from "../layout";

type RunAction = (action: () => Promise<void>) => Promise<void>;

interface ActiveMaterialsPanelProps {
  isOpen: boolean;
  selectedBodyId: string | null;
  selectedFaceId: string | null;
  runAction: RunAction;
  setBodyColor: (bodyId: string, color: string) => Promise<void>;
  setFaceColor: (faceId: string, color: string) => Promise<void>;
  clearBodyColor: (bodyId: string) => Promise<void>;
  clearFaceColor: (faceId: string) => Promise<void>;
  clearAppearanceOverrides: () => Promise<void>;
}

export function ActiveMaterialsPanel({
  isOpen,
  selectedBodyId,
  selectedFaceId,
  runAction,
  setBodyColor,
  setFaceColor,
  clearBodyColor,
  clearFaceColor,
  clearAppearanceOverrides,
}: ActiveMaterialsPanelProps) {
  if (!isOpen) {
    return null;
  }

  return (
    <div className="pointer-events-auto">
      <MaterialsPanel
        selectedBodyId={selectedBodyId}
        selectedFaceId={selectedFaceId}
        onApplyBodyColor={async (bodyId, color) => {
          await runAction(async () => {
            await setBodyColor(bodyId, color);
          });
        }}
        onApplyFaceColor={async (faceId, color) => {
          await runAction(async () => {
            await setFaceColor(faceId, color);
          });
        }}
        onClearBodyColor={async (bodyId) => {
          await runAction(async () => {
            await clearBodyColor(bodyId);
          });
        }}
        onClearFaceColor={async (faceId) => {
          await runAction(async () => {
            await clearFaceColor(faceId);
          });
        }}
        onClearAll={async () => {
          await runAction(async () => {
            await clearAppearanceOverrides();
          });
        }}
      />
    </div>
  );
}
