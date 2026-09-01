import type { AppConfig, SlicerViewportBounds } from "../lib";
import type { SlicerExportFormat } from "../types";
import { embedOrcaWindow, launchOrcaSlicer } from "../lib";
import { IS_MACOS, STANDALONE_SLICER_BOUNDS } from "./appState";

type AsyncVoid = () => Promise<void>;

export interface SlicerExportMessages {
  stepWebUnsupported: string;
  disabled: string;
  exporting: string;
  binaryMissing: string;
  containerUnavailable: string;
}

export interface SlicerExportContext {
  bodyId: string;
  exportFormat: SlicerExportFormat;
  orcaSlicer: AppConfig["orcaSlicer"];
  messages: SlicerExportMessages;
  setSlicerStatus: (status: string | null) => void;
  setWorkspaceView: (view: "slicer") => void;
  addMessage: (message: string) => void;
  prepareExportedSlicerFile: (
    format: SlicerExportFormat,
    bodyId: string,
  ) => Promise<string>;
  uploadStlToOrcaWeb: (stlPath: string) => Promise<void>;
  waitForNextFrame: AsyncVoid;
  readSlicerViewportBounds: () => SlicerViewportBounds | null;
  applyOrcaEmbedResult: (
    result: Awaited<ReturnType<typeof embedOrcaWindow>>,
    logPrefix: string,
    trackEmbedSession: boolean,
  ) => void;
  reportSlicerEmbedError: (error: unknown, logPrefix: string) => void;
}

export async function exportToSlicerFromContext(context: SlicerExportContext) {
  if (!context.orcaSlicer.enabled) {
    context.setSlicerStatus(context.messages.disabled);
    return;
  }

  if (
    context.orcaSlicer.integrationMode === "web" &&
    context.exportFormat === "step"
  ) {
    // The web integration uploads a mesh file; STEP is B-rep.
    context.setSlicerStatus(context.messages.stepWebUnsupported);
    return;
  }

  if (context.orcaSlicer.integrationMode === "web") {
    await exportToWebSlicer(context);
    return;
  }

  if (context.orcaSlicer.integrationMode === "external") {
    await exportToExternalSlicer(context);
    return;
  }

  await exportToNativeSlicer(context);
}

async function exportToExternalSlicer(context: SlicerExportContext) {
  const binaryPath = context.orcaSlicer.binaryPath.trim();
  if (!binaryPath) {
    context.setSlicerStatus(context.messages.binaryMissing);
    return;
  }

  try {
    context.setSlicerStatus(context.messages.exporting);
    const exportPath = await context.prepareExportedSlicerFile(
      context.exportFormat,
      context.bodyId,
    );
    const result = await launchOrcaSlicer({
      binaryPath,
      modelFilePath: exportPath,
    });
    context.applyOrcaEmbedResult(result, "slicer export", false);
  } catch (error) {
    context.reportSlicerEmbedError(error, "slicer export error");
  }
}

async function exportToWebSlicer(context: SlicerExportContext) {
  try {
    context.setSlicerStatus(context.messages.exporting);
    const exportPath = await context.prepareExportedSlicerFile(
      context.exportFormat,
      context.bodyId,
    );
    await context.uploadStlToOrcaWeb(exportPath);
    context.setWorkspaceView("slicer");
    context.setSlicerStatus(null);
    context.addMessage("slicer: exported STL to OrcaSlicer web.");
  } catch (error) {
    context.reportSlicerEmbedError(error, "slicer web export error");
  }
}

async function exportToNativeSlicer(context: SlicerExportContext) {
  const binaryPath = context.orcaSlicer.binaryPath.trim();
  if (!binaryPath) {
    context.setSlicerStatus(context.messages.binaryMissing);
    return;
  }

  try {
    if (IS_MACOS) {
      await exportToStandaloneMacSlicer(context, binaryPath);
      return;
    }
    await exportToEmbeddedNativeSlicer(context, binaryPath);
  } catch (error) {
    context.reportSlicerEmbedError(error, "slicer export error");
  }
}

async function exportToStandaloneMacSlicer(
  context: SlicerExportContext,
  binaryPath: string,
) {
  context.setSlicerStatus(context.messages.exporting);
  const exportPath = await context.prepareExportedSlicerFile(
    context.exportFormat,
    context.bodyId,
  );
  const result = await embedOrcaWindow({
    binaryPath,
    modelFilePath: exportPath,
    bounds: STANDALONE_SLICER_BOUNDS,
  });
  context.applyOrcaEmbedResult(result, "slicer export", false);
}

async function exportToEmbeddedNativeSlicer(
  context: SlicerExportContext,
  binaryPath: string,
) {
  context.setWorkspaceView("slicer");
  context.setSlicerStatus(context.messages.exporting);
  await context.waitForNextFrame();
  const bounds = context.readSlicerViewportBounds();
  if (!bounds) {
    context.setSlicerStatus(context.messages.containerUnavailable);
    return;
  }

  const exportPath = await context.prepareExportedSlicerFile(
    context.exportFormat,
    context.bodyId,
  );
  const result = await embedOrcaWindow({
    binaryPath,
    modelFilePath: exportPath,
    bounds,
  });
  context.applyOrcaEmbedResult(result, "slicer export", true);
}
