import { AppHeader } from "./components/AppHeader";
import { DocumentPanel } from "./components/DocumentPanel";
import { FeatureTimeline } from "./components/FeatureTimeline";
import { MessageLog } from "./components/MessageLog";
import { SelectedBoxEditor } from "./components/SelectedBoxEditor";
import { SessionPanel } from "./components/SessionPanel";
import { SketchToolPanel } from "./components/SketchToolPanel";
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
  const selectedReference =
    viewport?.reference_planes.find((referencePlane) => referencePlane.is_selected) ??
    null;
  const activeSketchPlaneId = document?.active_sketch_plane_id ?? null;
  const activeSketchTool = document?.active_sketch_tool ?? null;
  const sketchLineCount =
    document?.feature_history.find(
      (feature) => feature.feature_id === document.active_sketch_feature_id,
    )?.sketch_parameters?.lines.length ?? 0;
  const sketchCircleCount =
    document?.feature_history.find(
      (feature) => feature.feature_id === document.active_sketch_feature_id,
    )?.sketch_parameters?.circles.length ?? 0;
  const {
    start,
    ping,
    createDocument,
    refreshDocument,
    refreshSession,
    refreshViewport,
    addBoxFeature,
    addCylinderFeature,
    updateBoxFeature,
    renameFeature,
    deleteFeature,
    undo,
    redo,
    selectFeature,
    selectReference,
    startSketchOnPlane,
    setSketchTool,
    addSketchLine,
    addSketchRectangle,
    addSketchCircle,
    selectSketchEntity,
    finishSketch,
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
    <main className="cad-shell h-screen">
      <div className="grid h-full min-h-0 grid-rows-[auto_minmax(0,1fr)_auto]">
        <AppHeader
          status={status}
          disabled={status !== "connected"}
          canUndo={session?.can_undo ?? false}
          canRedo={session?.can_redo ?? false}
          activeSketchPlaneId={activeSketchPlaneId}
          activeSketchTool={activeSketchTool}
          selectedReferenceId={selectedReference?.reference_id ?? null}
          selectedReferenceLabel={selectedReference?.label ?? null}
          sketchLineCount={sketchLineCount}
          sketchCircleCount={sketchCircleCount}
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
          onAddBoxFeature={async (width, height, depth) => {
            await runAction(async () => {
              await addBoxFeature(width, height, depth);
            });
          }}
          onAddCylinderFeature={async (radius, height) => {
            await runAction(async () => {
              await addCylinderFeature(radius, height);
            });
          }}
          onStartSketch={async () => {
            if (!selectedReference) {
              return;
            }

            await runAction(async () => {
              await startSketchOnPlane(selectedReference.reference_id);
            });
          }}
          onFinishSketch={async () => {
            await runAction(async () => {
              await finishSketch();
            });
          }}
          onSetSketchTool={async (tool) => {
            await runAction(async () => {
              await setSketchTool(tool);
            });
          }}
        />

        <div className="grid min-h-0 min-w-0 grid-cols-[320px_minmax(0,1fr)]">
          <aside className="cad-sidebar min-h-0">
            <div className="flex h-full min-h-0 flex-col">
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
            </div>
          </aside>

          <section className="relative min-h-0 min-w-0">
            <ViewportPanel
              status={status}
              document={document}
              viewport={viewport}
              onSelectPrimitive={async (primitiveId) => {
                await runAction(async () => {
                  await selectFeature(primitiveId);
                });
              }}
              onSelectReference={async (referenceId) => {
                await runAction(async () => {
                  await selectReference(referenceId);
                });
              }}
              onStartSketch={async (referenceId) => {
                await runAction(async () => {
                  await startSketchOnPlane(referenceId);
                });
              }}
              onAddSketchLine={async (startX, startY, endX, endY) => {
                await runAction(async () => {
                  await addSketchLine(startX, startY, endX, endY);
                });
              }}
              onAddSketchRectangle={async (startX, startY, endX, endY) => {
                await runAction(async () => {
                  await addSketchRectangle(startX, startY, endX, endY);
                });
              }}
              onAddSketchCircle={async (centerX, centerY, radius) => {
                await runAction(async () => {
                  await addSketchCircle(centerX, centerY, radius);
                });
              }}
              onSelectSketchEntity={async (entityId) => {
                await runAction(async () => {
                  await selectSketchEntity(entityId);
                });
              }}
              onSetSketchTool={async (tool) => {
                await runAction(async () => {
                  await setSketchTool(tool);
                });
              }}
              onFinishSketch={async () => {
                await runAction(async () => {
                  await finishSketch();
                });
              }}
            />

            <div className="pointer-events-none absolute right-4 top-4 z-10 flex max-h-[calc(100%-1rem)] w-[340px] flex-col gap-3">
              {activeSketchPlaneId && activeSketchTool ? (
                <SketchToolPanel
                  activeSketchPlaneId={activeSketchPlaneId}
                  activeSketchTool={activeSketchTool}
                  selectedSketchEntityId={
                    document?.selected_sketch_entity_id ?? null
                  }
                />
              ) : null}
              <SelectedBoxEditor
                feature={selectedFeature}
                selectedSketchEntityId={document?.selected_sketch_entity_id ?? null}
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
              <SessionPanel session={session} />
              <MessageLog messages={messages} />
            </div>
          </section>
        </div>

        <FeatureTimeline
          document={document}
          onSelectFeature={async (featureId) => {
            await runAction(async () => {
              await selectFeature(featureId);
            });
          }}
        />
      </div>
    </main>
  );
}

export default App;
