import { useEffect, useMemo, useState } from "react";
import { open, save } from "@tauri-apps/plugin-dialog";
import { awaitDocumentChange, useCadCoreStore } from "./state";
import { useCadCore } from "./hooks";
import {
  AppHeader,
  DocumentHierarchyPanel,
  ExtrudePreviewPanel,
  FeatureTimeline,
  MessageLog,
  SketchToolPanel,
  ViewportPanel,
} from "./layout";
import type { CategoryId } from "./layout";
import { ArmedSketchConstraint } from "./types";

const DEFAULT_EXTRUDE_DEPTH = 20;

// The Core Messages debug panel is hidden by default. Set
// `VITE_SHOW_DEBUG_MESSAGE_LOG=true` in `.env.local` (or your shell when
// running `pnpm dev`) to surface it again while debugging the IPC bridge.
const SHOW_DEBUG_MESSAGE_LOG =
  import.meta.env.VITE_SHOW_DEBUG_MESSAGE_LOG === "true";

interface ActiveExtrudeAction {
  featureId: string;
  initialDepth: number;
}

function App() {
  const [armedSketchConstraint, setArmedSketchConstraint] =
    useState<ArmedSketchConstraint>(null);
  const [extrudeAction, setExtrudeAction] =
    useState<ActiveExtrudeAction | null>(null);
  const [hiddenFeatureIds, setHiddenFeatureIds] = useState<Set<string>>(
    () => new Set<string>(),
  );
  const [hiddenCategories, setHiddenCategories] = useState<Set<CategoryId>>(
    () => new Set<CategoryId>(),
  );
  const status = useCadCoreStore((state) => state.status);
  const messages = useCadCoreStore((state) => state.messages);
  const document = useCadCoreStore((state) => state.document);
  const session = useCadCoreStore((state) => state.session);
  const viewport = useCadCoreStore((state) => state.viewport);
  const addMessage = useCadCoreStore((state) => state.addMessage);
  const selectedReference =
    viewport?.reference_planes.find(
      (referencePlane) => referencePlane.is_selected,
    ) ?? null;
  const selectedSketchProfile =
    viewport?.sketch_profiles.find((profile) => profile.is_selected) ?? null;
  const activeSketchPlaneId = document?.active_sketch_plane_id ?? null;
  const activeSketchTool = document?.active_sketch_tool ?? null;
  const activeSketchFeature =
    document?.feature_history.find(
      (feature) => feature.feature_id === document.active_sketch_feature_id,
    ) ?? null;
  const sketchLineCount =
    activeSketchFeature?.sketch_parameters?.lines.length ?? 0;
  const sketchCircleCount =
    activeSketchFeature?.sketch_parameters?.circles.length ?? 0;

  function toCorePlaneFrame(planeFrame: {
    origin: [number, number, number];
    xAxis: [number, number, number];
    yAxis: [number, number, number];
    normal: [number, number, number];
  }) {
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
  const {
    start,
    createDocument,
    exportDocument,
    exportDocumentStl,
    saveDocument,
    loadDocument,
    projectFaceIntoSketch,
    addBoxFeature,
    addCylinderFeature,
    updateExtrudeDepth,
    renameFeature,
    deleteFeature,
    undo,
    redo,
    selectFeature,
    selectReference,
    selectFace,
    startSketchOnPlane,
    startSketchOnFace,
    setSketchTool,
    setSketchLineConstraint,
    setSketchEqualLengthConstraint,
    setSketchCoincidentConstraint,
    setSketchParallelConstraint,
    setSketchPerpendicularConstraint,
    setSketchPointFixed,
    updateSketchDimension,
    selectSketchProfile,
    extrudeProfile,
    addSketchLine,
    addSketchRectangle,
    addSketchCircle,
    selectSketchPoint,
    selectSketchEntity,
    selectSketchDimension,
    finishSketch,
    reenterSketch,
  } = useCadCore();

  useEffect(() => {
    if (!activeSketchPlaneId) {
      setArmedSketchConstraint(null);
    }
  }, [activeSketchPlaneId]);

  // UI-only visibility: combine per-feature hides with category hides into
  // sets the viewport can use to filter primitives, sketch entities, and
  // reference geometry. Sketch entities are filtered by plane id since the
  // viewport snapshot does not carry the owning sketch feature id on each
  // sketch primitive.
  const BODY_KINDS = new Set(["box", "cylinder", "polygon_extrude", "extrude"]);
  const effectiveHiddenFeatureIds = useMemo(() => {
    const set = new Set<string>(hiddenFeatureIds);
    if (!document) {
      return set;
    }
    for (const feature of document.feature_history) {
      if (hiddenCategories.has("sketches") && feature.kind === "sketch") {
        set.add(feature.feature_id);
      }
      if (hiddenCategories.has("bodies") && BODY_KINDS.has(feature.kind)) {
        set.add(feature.feature_id);
      }
    }
    return set;
  }, [document, hiddenFeatureIds, hiddenCategories]);

  const hiddenSketchPlaneIds = useMemo(() => {
    const result = new Set<string>();
    if (!document) {
      return result;
    }
    // Group sketch features by plane id so we only hide a plane when every
    // sketch attached to it is hidden.
    const planeToSketches = new Map<string, string[]>();
    for (const feature of document.feature_history) {
      if (feature.kind !== "sketch" || !feature.sketch_parameters) {
        continue;
      }
      const planeId = feature.sketch_parameters.plane_id;
      const list = planeToSketches.get(planeId) ?? [];
      list.push(feature.feature_id);
      planeToSketches.set(planeId, list);
    }
    for (const [planeId, sketchIds] of planeToSketches) {
      if (sketchIds.every((id) => effectiveHiddenFeatureIds.has(id))) {
        result.add(planeId);
      }
    }
    return result;
  }, [document, effectiveHiddenFeatureIds]);

  async function triggerExtrudeAction() {
    if (extrudeAction) {
      return;
    }

    if (!selectedSketchProfile) {
      return;
    }

    const profileId = selectedSketchProfile.profile_id;

    // The IPC bridge is fire-and-forget: `extrudeProfile` returns as soon as
    // the command is written to cad_core stdin, before the core has emitted
    // the `document_state` event with the new feature. To capture the real
    // new feature id we subscribe to the next document update that contains
    // a freshly created extrude feature.
    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "extrude"
      );
    });

    await runAction(async () => {
      await extrudeProfile(profileId, DEFAULT_EXTRUDE_DEPTH);
      try {
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (!newFeatureId) {
          return;
        }
        setExtrudeAction({
          featureId: newFeatureId,
          initialDepth: DEFAULT_EXTRUDE_DEPTH,
        });
      } catch (error) {
        addMessage(`extrude action error: ${String(error)}`);
      }
    });
  }

  useEffect(() => {
    function isTypingTarget(target: EventTarget | null) {
      return (
        target instanceof HTMLInputElement ||
        target instanceof HTMLTextAreaElement ||
        target instanceof HTMLSelectElement ||
        (target instanceof HTMLElement && target.isContentEditable)
      );
    }

    function handleKeyDown(event: KeyboardEvent) {
      const target = event.target;
      if (isTypingTarget(target)) {
        return;
      }

      const isMod = event.metaKey || event.ctrlKey;

      // Undo: Cmd/Ctrl+Z (no Shift). Redo: Cmd/Ctrl+Shift+Z, or Cmd/Ctrl+Y.
      if (isMod && !event.altKey && event.code === "KeyZ") {
        event.preventDefault();
        if (event.shiftKey) {
          if (session?.can_redo) {
            void runAction(redo);
          }
        } else {
          if (session?.can_undo) {
            void runAction(undo);
          }
        }
        return;
      }

      if (isMod && !event.altKey && !event.shiftKey && event.code === "KeyY") {
        event.preventDefault();
        if (session?.can_redo) {
          void runAction(redo);
        }
        return;
      }

      // E: trigger extrude action (no modifiers).
      if (event.ctrlKey || event.metaKey || event.altKey || event.shiftKey) {
        return;
      }

      if (event.code !== "KeyE") {
        return;
      }

      if (!selectedSketchProfile) {
        return;
      }

      event.preventDefault();
      void triggerExtrudeAction();
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [
    selectedSketchProfile,
    extrudeAction,
    session?.can_undo,
    session?.can_redo,
  ]);

  function clearArmedSketchConstraint() {
    setArmedSketchConstraint(null);
  }

  async function handleSketchConstraintLinePick(lineId: string) {
    if (!armedSketchConstraint) {
      await selectSketchEntity(lineId);
      return;
    }

    if (armedSketchConstraint.kind === "coincident") {
      await selectSketchEntity(lineId);
      return;
    }

    if (armedSketchConstraint.kind === "horizontal") {
      await setSketchLineConstraint(lineId, "horizontal");
      clearArmedSketchConstraint();
      return;
    }

    if (armedSketchConstraint.kind === "vertical") {
      await setSketchLineConstraint(lineId, "vertical");
      clearArmedSketchConstraint();
      return;
    }

    if (armedSketchConstraint.kind === "clear") {
      await setSketchLineConstraint(lineId, "none");
      clearArmedSketchConstraint();
      return;
    }

    const firstLineId =
      "firstLineId" in armedSketchConstraint
        ? armedSketchConstraint.firstLineId
        : null;

    if (!firstLineId) {
      await selectSketchEntity(lineId);
      setArmedSketchConstraint({
        kind: armedSketchConstraint.kind,
        firstLineId: lineId,
      });
      return;
    }

    if (firstLineId === lineId) {
      return;
    }

    if (armedSketchConstraint.kind === "equal_length") {
      await setSketchEqualLengthConstraint(lineId, firstLineId);
    } else if (armedSketchConstraint.kind === "parallel") {
      await setSketchParallelConstraint(lineId, firstLineId);
    } else {
      await setSketchPerpendicularConstraint(lineId, firstLineId);
    }
    clearArmedSketchConstraint();
  }

  async function handleSketchConstraintPointPick(
    pointId: string,
    kind: "endpoint" | "center",
  ) {
    if (!armedSketchConstraint || armedSketchConstraint.kind !== "coincident") {
      await selectSketchPoint(pointId);
      return;
    }

    if (kind !== "endpoint") {
      await selectSketchPoint(pointId);
      return;
    }

    if (!armedSketchConstraint.firstPointId) {
      await selectSketchPoint(pointId);
      setArmedSketchConstraint({
        kind: "coincident",
        firstPointId: pointId,
      });
      return;
    }

    if (armedSketchConstraint.firstPointId === pointId) {
      return;
    }

    await setSketchCoincidentConstraint(
      pointId,
      armedSketchConstraint.firstPointId,
    );
    clearArmedSketchConstraint();
  }

  function makeDefaultExportBaseName() {
    return (
      (document?.name ?? "polysmith-part")
        .trim()
        .toLowerCase()
        .replace(/[^a-z0-9]+/g, "-")
        .replace(/^-+|-+$/g, "") || "polysmith-part"
    );
  }

  async function pickExportPath() {
    const filePath = await save({
      title: "Export STEP",
      defaultPath: `${makeDefaultExportBaseName()}.step`,
      filters: [
        {
          name: "STEP",
          extensions: ["step", "stp"],
        },
      ],
    });

    if (filePath === null) {
      addMessage("export canceled");
      return null;
    }

    return filePath;
  }

  async function pickExportStlPath() {
    const filePath = await save({
      title: "Export STL",
      defaultPath: `${makeDefaultExportBaseName()}.stl`,
      filters: [
        {
          name: "STL",
          extensions: ["stl"],
        },
      ],
    });

    if (filePath === null) {
      addMessage("export canceled");
      return null;
    }

    return filePath;
  }

  async function pickSaveDocumentPath() {
    const filePath = await save({
      title: "Save PolySmith document",
      defaultPath: `${makeDefaultExportBaseName()}.polysmith`,
      filters: [
        {
          name: "PolySmith document",
          extensions: ["polysmith", "json"],
        },
      ],
    });

    if (filePath === null) {
      addMessage("save canceled");
      return null;
    }
    return filePath;
  }

  async function pickLoadDocumentPath() {
    const result = await open({
      title: "Open PolySmith document",
      multiple: false,
      directory: false,
      filters: [
        {
          name: "PolySmith document",
          extensions: ["polysmith", "json"],
        },
      ],
    });

    if (result === null || Array.isArray(result)) {
      addMessage("open canceled");
      return null;
    }
    return result;
  }

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
          armedSketchConstraint={armedSketchConstraint}
          onStart={async () => {
            await runAction(start);
          }}
          onCreateDocument={async () => {
            await runAction(createDocument);
          }}
          onExportDocument={async () => {
            const filePath = await pickExportPath();
            if (!filePath) {
              return;
            }

            await runAction(async () => {
              await exportDocument(filePath);
              addMessage(`export requested: ${filePath}`);
            });
          }}
          onExportDocumentStl={async () => {
            const filePath = await pickExportStlPath();
            if (!filePath) {
              return;
            }

            await runAction(async () => {
              await exportDocumentStl(filePath);
              addMessage(`stl export requested: ${filePath}`);
            });
          }}
          onSaveDocument={async () => {
            const filePath = await pickSaveDocumentPath();
            if (!filePath) {
              return;
            }

            await runAction(async () => {
              await saveDocument(filePath);
              addMessage(`saved: ${filePath}`);
            });
          }}
          onLoadDocument={async () => {
            const filePath = await pickLoadDocumentPath();
            if (!filePath) {
              return;
            }

            await runAction(async () => {
              await loadDocument(filePath);
              addMessage(`loaded: ${filePath}`);
            });
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
              clearArmedSketchConstraint();
              await finishSketch();
            });
          }}
          onSetSketchTool={async (tool) => {
            await runAction(async () => {
              clearArmedSketchConstraint();
              await setSketchTool(tool);
            });
          }}
          onArmSketchConstraint={async (constraint) => {
            let shouldArm = true;

            setArmedSketchConstraint((current) => {
              const isSameConstraint =
                current &&
                current.kind === constraint &&
                (constraint !== "equal_length" &&
                constraint !== "coincident" &&
                constraint !== "perpendicular" &&
                constraint !== "parallel"
                  ? true
                  : current.kind === constraint);

              if (isSameConstraint) {
                shouldArm = false;
                return null;
              }

              return constraint === "equal_length" ||
                constraint === "coincident" ||
                constraint === "perpendicular" ||
                constraint === "parallel"
                ? constraint === "coincident"
                  ? { kind: constraint, firstPointId: null }
                  : { kind: constraint, firstLineId: null }
                : ({ kind: constraint } as ArmedSketchConstraint);
            });

            if (shouldArm && activeSketchTool !== "select") {
              await runAction(async () => {
                await setSketchTool("select");
              });
            }
          }}
          onCancelSketchConstraint={clearArmedSketchConstraint}
        />

        <div className="grid min-h-0 min-w-0 grid-cols-[320px_minmax(0,1fr)]">
          <aside className="cad-sidebar min-h-0">
            <div className="flex h-full min-h-0 flex-col">
              <DocumentHierarchyPanel
                document={document}
                hiddenFeatureIds={hiddenFeatureIds}
                hiddenCategories={hiddenCategories}
                onToggleFeatureVisibility={(featureId) => {
                  setHiddenFeatureIds((current) => {
                    const next = new Set(current);
                    if (next.has(featureId)) {
                      next.delete(featureId);
                    } else {
                      next.add(featureId);
                    }
                    return next;
                  });
                }}
                onToggleCategoryVisibility={(category) => {
                  setHiddenCategories((current) => {
                    const next = new Set(current);
                    if (next.has(category)) {
                      next.delete(category);
                    } else {
                      next.add(category);
                    }
                    return next;
                  });
                }}
                onSelectFeature={async (featureId) => {
                  await runAction(async () => {
                    await selectFeature(featureId);
                  });
                }}
                onReenterSketch={async (featureId) => {
                  await runAction(async () => {
                    await reenterSketch(featureId);
                  });
                }}
                onRenameFeature={async (featureId, name) => {
                  await runAction(async () => {
                    await renameFeature(featureId, name);
                  });
                }}
                onDeleteFeature={async (featureId) => {
                  await runAction(async () => {
                    await deleteFeature(featureId);
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
              onSelectFace={async (faceId) => {
                await runAction(async () => {
                  await selectFace(faceId);
                });
              }}
              onStartSketch={async (referenceId) => {
                await runAction(async () => {
                  await startSketchOnPlane(referenceId);
                });
              }}
              onStartSketchOnFace={async (faceId, planeFrame) => {
                await runAction(async () => {
                  await startSketchOnFace(faceId, toCorePlaneFrame(planeFrame));
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
                  await handleSketchConstraintLinePick(entityId);
                });
              }}
              onPickSketchPoint={async (pointId, kind) => {
                await runAction(async () => {
                  await handleSketchConstraintPointPick(pointId, kind);
                });
              }}
              armedSketchConstraint={armedSketchConstraint}
              onCancelSketchConstraint={clearArmedSketchConstraint}
              onClearSketchConstraint={async (
                kind,
                entityId,
                _relatedEntityId,
              ) => {
                await runAction(async () => {
                  if (kind === "fixed") {
                    await setSketchPointFixed(entityId, false);
                    return;
                  }

                  if (kind === "equal_length") {
                    await setSketchEqualLengthConstraint(entityId, null);
                    return;
                  }

                  if (kind === "perpendicular") {
                    await setSketchPerpendicularConstraint(entityId, null);
                    return;
                  }

                  if (kind === "parallel") {
                    await setSketchParallelConstraint(entityId, null);
                    return;
                  }

                  await setSketchLineConstraint(entityId, "none");
                });
              }}
              onSelectSketchDimension={async (dimensionId) => {
                await runAction(async () => {
                  await selectSketchDimension(dimensionId);
                });
              }}
              onUpdateSketchDimension={async (dimensionId, value) => {
                await runAction(async () => {
                  await updateSketchDimension(dimensionId, value);
                });
              }}
              onSelectSketchProfile={async (profileId) => {
                await runAction(async () => {
                  await selectSketchProfile(profileId);
                });
              }}
              onSetSketchTool={async (tool) => {
                await runAction(async () => {
                  clearArmedSketchConstraint();
                  await setSketchTool(tool);
                });
              }}
              hiddenFeatureIds={effectiveHiddenFeatureIds}
              hiddenSketchPlaneIds={hiddenSketchPlaneIds}
              hideReferences={hiddenCategories.has("origin")}
            />

            <div className="pointer-events-none absolute right-4 top-4 z-10 flex max-h-[calc(100%-1rem)] w-[340px] flex-col gap-3">
              {activeSketchPlaneId && activeSketchTool ? (
                <SketchToolPanel
                  activeSketchPlaneId={activeSketchPlaneId}
                  activeSketchTool={activeSketchTool}
                  selectedSketchPointId={
                    document?.selected_sketch_point_id ?? null
                  }
                  selectedSketchEntityId={
                    document?.selected_sketch_entity_id ?? null
                  }
                  selectedSketchProfileId={
                    document?.selected_sketch_profile_id ?? null
                  }
                  selectedFaceId={document?.selected_face_id ?? null}
                  onProjectFace={async () => {
                    const faceId = document?.selected_face_id ?? null;
                    if (!faceId) {
                      return;
                    }
                    await runAction(async () => {
                      await projectFaceIntoSketch(faceId);
                    });
                  }}
                />
              ) : null}
              {extrudeAction ? (
                <ExtrudePreviewPanel
                  initialDepth={extrudeAction.initialDepth}
                  disabled={status !== "connected"}
                  onPreviewDepth={async (depth) => {
                    await runAction(async () => {
                      await updateExtrudeDepth(extrudeAction.featureId, depth);
                    });
                  }}
                  onConfirm={() => {
                    setExtrudeAction(null);
                  }}
                  onCancel={async () => {
                    await runAction(async () => {
                      await undo();
                    });
                    setExtrudeAction(null);
                  }}
                />
              ) : null}
              {SHOW_DEBUG_MESSAGE_LOG ? (
                <MessageLog messages={messages} />
              ) : null}
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
