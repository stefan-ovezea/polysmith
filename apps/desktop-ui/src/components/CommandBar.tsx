interface CommandBarProps {
  disabled: boolean;
  onStart: () => Promise<void>;
  onPing: () => Promise<void>;
  onCreateDocument: () => Promise<void>;
  onRefreshDocument: () => Promise<void>;
  onRefreshSession: () => Promise<void>;
  onRefreshViewport: () => Promise<void>;
  onUndo: () => Promise<void>;
  onRedo: () => Promise<void>;
  canUndo: boolean;
  canRedo: boolean;
}

export function CommandBar({
  disabled,
  onStart,
  onPing,
  onCreateDocument,
  onRefreshDocument,
  onRefreshSession,
  onRefreshViewport,
  onUndo,
  onRedo,
  canUndo,
  canRedo,
}: CommandBarProps) {
  return (
    <div className="cad-panel-soft flex flex-wrap items-center gap-3 px-4 py-4">
      <button className="cad-action-primary" onClick={() => void onStart()}>
        Start Core
      </button>
      <button className="cad-action-ghost" onClick={() => void onPing()} disabled={disabled}>
        Ping
      </button>
      <button className="cad-action-ghost" onClick={() => void onCreateDocument()} disabled={disabled}>
        New Document
      </button>
      <button className="cad-action-ghost" onClick={() => void onRefreshDocument()} disabled={disabled}>
        Sync Document
      </button>
      <button className="cad-action-ghost" onClick={() => void onRefreshSession()} disabled={disabled}>
        Sync Session
      </button>
      <button className="cad-action-ghost" onClick={() => void onRefreshViewport()} disabled={disabled}>
        Sync Viewport
      </button>
      <div className="ml-auto flex gap-2">
        <button className="cad-action-ghost" onClick={() => void onUndo()} disabled={disabled || !canUndo}>
          Undo
        </button>
        <button className="cad-action-ghost" onClick={() => void onRedo()} disabled={disabled || !canRedo}>
          Redo
        </button>
      </div>
    </div>
  );
}
