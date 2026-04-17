import { useEffect, useState } from "react";
import { save } from "@tauri-apps/plugin-dialog";
import { AppHeader } from "./components/AppHeader";
import { DocumentPanel } from "./components/DocumentPanel";
import { FeatureTimeline } from "./components/FeatureTimeline";
import { MessageLog } from "./components/MessageLog";
import { SelectedBoxEditor } from "./components/SelectedBoxEditor";
import { SketchToolPanel } from "./components/SketchToolPanel";
import { ViewportPanel } from "./components/ViewportPanel";
import { useCadCore } from "./hooks/useCadCore";
import { useCadCoreStore } from "./state/cadCoreStore";

function App() {
  const [armedSketchConstraint, setArmedSketchConstraint] = useState<
    | null
    | {
        kind: "horizontal" | "vertical" | "clear";
      }
    | {
        kind: "equal_length" | "perpendicular" | "parallel";
        firstLineId: string | null;
      }
    | {
        kind: "coincident";
        firstPointId: string | null;
      }
  >(null);
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
  const sketchLineCount = activeSketchFeature?.sketch_parameters?.lines.length ?? 0;
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
    updateSketchCircle,
    updateSketchDimension,
    selectSketchProfile,
    extrudeProfile,
    addSketchLine,
    addSketchRectangle,
    addSketchCircle,
    selectSketchEntity,
    selectSketchDimension,
    finishSketch,
    clearSelection,
  } = useCadCore();

  useEffect(() => {
    if (!activeSketchPlaneId) {
      setArmedSketchConstraint(null);
    }
  }, [activeSketchPlaneId]);

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
    entityId: string,
    kind: "endpoint" | "center",
  ) {
    if (!armedSketchConstraint || armedSketchConstraint.kind !== "coincident") {
      await selectSketchEntity(entityId);
      return;
    }

    if (kind !== "endpoint") {
      return;
    }

    if (!armedSketchConstraint.firstPointId) {
      await selectSketchEntity(entityId);
      setArmedSketchConstraint({
        kind: "coincident",
        firstPointId: pointId,
      });
      return;
    }

    if (armedSketchConstraint.firstPointId === pointId) {
      return;
    }

    await setSketchCoincidentConstraint(pointId, armedSketchConstraint.firstPointId);
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
                : { kind: constraint };
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
              onPickSketchPoint={async (pointId, entityId, kind) => {
                await runAction(async () => {
                  await handleSketchConstraintPointPick(pointId, entityId, kind);
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
                  selectedSketchProfileId={
                    document?.selected_sketch_profile_id ?? null
                  }
                  onExtrudeProfile={async (depth) => {
                    if (!selectedSketchProfile) {
                      return;
                    }

                    await runAction(async () => {
                      await extrudeProfile(selectedSketchProfile.profile_id, depth);
                    });
                  }}
                />
              ) : null}
              <SelectedBoxEditor
                feature={selectedFeature}
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
