import { useEffect, useMemo, useState } from "react";
import { save } from "@tauri-apps/plugin-dialog";
import { useCadCoreStore } from "./state";
import { useCadCore } from "./hooks";
import {
  AppHeader,
  DocumentHierarchyPanel,
  ExtrudePreviewPanel,
  FeatureTimeline,
  MessageLog,
  SelectedBoxEditor,
  SketchToolPanel,
  ViewportPanel,
} from "./layout";
import type { CategoryId } from "./layout";
import { ArmedSketchConstraint } from "./types";

const DEFAULT_EXTRUDE_DEPTH = 20;

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
  const selectedFeature =
    document?.feature_history.find(
      (feature) => feature.feature_id === document.selected_feature_id,
    ) ?? null;
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
    addBoxFeature,
    addCylinderFeature,
    updateBoxFeature,
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
    updateSketchLine,
    setSketchLineConstraint,
    setSketchEqualLengthConstraint,
    setSketchCoincidentConstraint,
    setSketchParallelConstraint,
    setSketchPerpendicularConstraint,
    setSketchPointFixed,
    updateSketchCircle,
    updateSketchDimension,
    updateSketchPoint,
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
    await runAction(async () => {
      await extrudeProfile(profileId, DEFAULT_EXTRUDE_DEPTH);
    });

    // The core selects the new extrude feature on success. Read the latest
    // selection from the store so the preview panel can drive depth updates.
    const currentDocument = useCadCoreStore.getState().document;
    const newFeatureId = currentDocument?.selected_feature_id ?? null;
    if (!newFeatureId) {
      return;
    }

    setExtrudeAction({
      featureId: newFeatureId,
      initialDepth: DEFAULT_EXTRUDE_DEPTH,
    });
  }

  useEffect(() => {
    function handleKeyDown(event: KeyboardEvent) {
      const target = event.target;
      if (
        target instanceof HTMLInputElement ||
        target instanceof HTMLTextAreaElement ||
        target instanceof HTMLSelectElement ||
        (target instanceof HTMLElement && target.isContentEditable)
      ) {
        return;
      }

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
  }, [selectedSketchProfile, extrudeAction]);

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

  function makeDefaultExportPath() {
    const baseName =
      (document?.name ?? "polysmith-part")
        .trim()
        .toLowerCase()
        .replace(/[^a-z0-9]+/g, "-")
        .replace(/^-+|-+$/g, "") || "polysmith-part";

    return `${baseName}.step`;
  }

  async function pickExportPath() {
    const filePath = await save({
      title: "Export STEP",
      defaultPath: makeDefaultExportPath(),
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
              <SelectedBoxEditor
                feature={selectedFeature}
                selectedSketchPointId={
                  document?.selected_sketch_point_id ?? null
                }
                selectedSketchEntityId={
                  document?.selected_sketch_entity_id ?? null
                }
                selectedSketchDimensionId={
                  document?.selected_sketch_dimension_id ?? null
                }
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
                onUpdateSketchLine={async (
                  lineId,
                  startX,
                  startY,
                  endX,
                  endY,
                ) => {
                  await runAction(async () => {
                    await updateSketchLine(lineId, startX, startY, endX, endY);
                  });
                }}
                onUpdateSketchCircle={async (
                  circleId,
                  centerX,
                  centerY,
                  radius,
                ) => {
                  await runAction(async () => {
                    await updateSketchCircle(
                      circleId,
                      centerX,
                      centerY,
                      radius,
                    );
                  });
                }}
                onUpdateSketchDimension={async (dimensionId, value) => {
                  await runAction(async () => {
                    await updateSketchDimension(dimensionId, value);
                  });
                }}
                onUpdateSketchPoint={async (pointId, x, y) => {
                  await runAction(async () => {
                    await updateSketchPoint(pointId, x, y);
                  });
                }}
                onSetSketchPointFixed={async (pointId, isFixed) => {
                  await runAction(async () => {
                    await setSketchPointFixed(pointId, isFixed);
                  });
                }}
              />
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
