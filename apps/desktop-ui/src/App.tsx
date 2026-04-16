import { AppHeader } from "./components/AppHeader";
import { BoxFeatureForm } from "./components/BoxFeatureForm";
import { CommandBar } from "./components/CommandBar";
import { DocumentPanel } from "./components/DocumentPanel";
import { FeatureTimeline } from "./components/FeatureTimeline";
import { MessageLog } from "./components/MessageLog";
import { SelectedBoxEditor } from "./components/SelectedBoxEditor";
import { SessionPanel } from "./components/SessionPanel";
import { ViewportPanel } from "./components/ViewportPanel";
import { useCadCore } from "./hooks/useCadCore";
import { useCadCoreStore } from "./state/cadCoreStore";

function App() {
  const status = useCadCoreStore((state) => state.status);
  const messages = useCadCoreStore((state) => state.messages);
  const document = useCadCoreStore((state) => state.document);
  const session = useCadCoreStore((state) => state.session);
  const viewport = useCadCoreStore((state) => state.viewport);
  const addMessage = useCadCoreStore((state) => state.addMessage);
  const selectedFeature =
    document?.feature_history.find(
      (feature) => feature.feature_id === document.selected_feature_id,
    ) ?? null;
  const {
    start,
    ping,
    createDocument,
    refreshDocument,
    refreshSession,
    refreshViewport,
    addBoxFeature,
    updateBoxFeature,
    renameFeature,
    deleteFeature,
    undo,
    redo,
    selectFeature,
    clearSelection,
  } =
    useCadCore();

  async function runAction(action: () => Promise<void>) {
    try {
      await action();
    } catch (error) {
      addMessage(`action error: ${String(error)}`);
    }
  }

  return (
    <main className="cad-shell relative">
      <AppHeader />

      <div className="relative flex min-h-screen gap-6 px-6 pb-28 pt-28">
        <aside className="relative z-10 flex w-[320px] shrink-0 flex-col gap-4">
          <DocumentPanel
            document={document}
            onSelectFeature={async (featureId) => {
              await runAction(async () => {
                await selectFeature(featureId);
              });
            }}
            onClearSelection={async () => {
              await runAction(async () => {
                await clearSelection();
              });
            }}
          />
          <SessionPanel session={session} />
          <MessageLog messages={messages} />
        </aside>

        <section className="relative z-0 flex min-w-0 flex-1 flex-col gap-4">
          <CommandBar
            disabled={status !== "connected"}
            onStart={async () => {
              await runAction(start);
            }}
            onPing={async () => {
              await runAction(ping);
            }}
            onCreateDocument={async () => {
              await runAction(createDocument);
            }}
            onRefreshDocument={async () => {
              await runAction(refreshDocument);
            }}
            onRefreshSession={async () => {
              await runAction(refreshSession);
            }}
            onRefreshViewport={async () => {
              await runAction(refreshViewport);
            }}
            onUndo={async () => {
              await runAction(undo);
            }}
            onRedo={async () => {
              await runAction(redo);
            }}
            canUndo={session?.can_undo ?? false}
            canRedo={session?.can_redo ?? false}
          />
          <div className="relative min-h-[720px] flex-1">
            <ViewportPanel
              viewport={viewport}
              onSelectPrimitive={async (primitiveId) => {
                await runAction(async () => {
                  await selectFeature(primitiveId);
                });
              }}
            />
          </div>
        </section>

        <aside className="relative z-10 flex w-[360px] shrink-0 flex-col gap-4">
          <BoxFeatureForm
            disabled={status !== "connected"}
            onSubmit={async (width, height, depth) => {
              await runAction(async () => {
                await addBoxFeature(width, height, depth);
              });
            }}
          />
          <SelectedBoxEditor
            feature={selectedFeature}
            disabled={status !== "connected"}
            onSubmit={async (featureId, width, height, depth) => {
              await runAction(async () => {
                await updateBoxFeature(featureId, width, height, depth);
              });
            }}
            onRename={async (featureId, name) => {
              await runAction(async () => {
                await renameFeature(featureId, name);
              });
            }}
            onDelete={async (featureId) => {
              await runAction(async () => {
                await deleteFeature(featureId);
              });
            }}
          />
        </aside>
      </div>

      <FeatureTimeline
        document={document}
        onSelectFeature={async (featureId) => {
          await runAction(async () => {
            await selectFeature(featureId);
          });
        }}
      />
    </main>
  );
}

export default App;
