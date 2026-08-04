import { ViewPanel } from "../layout";

interface ActiveViewPanelProps {
  isOpen: boolean;
}

export function ActiveViewPanel({ isOpen }: ActiveViewPanelProps) {
  if (!isOpen) {
    return null;
  }

  return (
    <div className="pointer-events-auto">
      <ViewPanel />
    </div>
  );
}
