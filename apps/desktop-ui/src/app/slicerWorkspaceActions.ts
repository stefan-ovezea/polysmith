import { useEffect, useRef } from "react";
import type { RefObject } from "react";

import { awaitDocumentExport } from "../state";
import type { AppConfig, SlicerViewportBounds } from "../lib";
import {
  embedOrcaWindow,
  hideOrcaWindow,
  prepareOrcaExportPath,
  resizeOrcaWindow,
  setOrcaMapped,
} from "../lib";
import type { WorkspaceView } from "./appState";
import { exportToSlicerFromContext } from "./slicerExport";

interface SlicerWorkspaceActionMessages {
  disabled: string;
  opening: string;
  binaryMissing: string;
  containerUnavailable: string;
  embedFailed: (error: string) => string;
  noExportableBody: string;
  exporting: string;
}

interface SlicerWorkspaceActionContext {
  workspaceView: WorkspaceView;
  hasExportableBody: boolean;
  hasOrcaEmbedSession: boolean;
  orcaSlicer: AppConfig["orcaSlicer"];
  slicerViewportRef: RefObject<HTMLDivElement | null>;
  messages: SlicerWorkspaceActionMessages;
  setWorkspaceView: (view: WorkspaceView) => void;
  setSlicerStatus: (status: string | null) => void;
  setHasOrcaEmbedSession: (hasSession: boolean) => void;
  exportDocumentStl: (filePath: string) => Promise<void>;
  addMessage: (message: string) => void;
}

export function useSlicerWorkspaceActions({
  workspaceView,
  hasExportableBody,
  hasOrcaEmbedSession,
  orcaSlicer,
  slicerViewportRef,
  messages,
  setWorkspaceView,
  setSlicerStatus,
  setHasOrcaEmbedSession,
  exportDocumentStl,
  addMessage,
}: SlicerWorkspaceActionContext) {
  const workspaceViewRef = useRef(workspaceView);
  workspaceViewRef.current = workspaceView;

  function readSlicerViewportBounds(): SlicerViewportBounds | null {
    const container = slicerViewportRef.current;
    if (!container) {
      return null;
    }

    const rect = container.getBoundingClientRect();
    if (rect.width < 1 || rect.height < 1) {
      return null;
    }

    return {
      x: rect.left,
      y: rect.top,
      width: rect.width,
      height: rect.height,
      scaleFactor: window.devicePixelRatio || 1,
    };
  }

  function waitForNextFrame() {
    return new Promise<void>((resolve) => {
      window.requestAnimationFrame(() => resolve());
    });
  }

  async function showCadView() {
    setWorkspaceView("cad");
    setSlicerStatus(null);
    if (!hasOrcaEmbedSession) {
      return;
    }
    try {
      const result = await hideOrcaWindow();
      addMessage(`slicer: ${result.message}`);
    } catch (error) {
      addMessage(`slicer hide error: ${String(error)}`);
    } finally {
      setHasOrcaEmbedSession(false);
    }
  }

  async function showCamView() {
    setWorkspaceView("cam");
  }

  async function prepareExportedSlicerStl() {
    const exportPath = await prepareOrcaExportPath();
    await exportDocumentStl(exportPath);
    await awaitDocumentExport(
      (result) => result.format === "stl" && result.file_path === exportPath,
    );
    return exportPath;
  }

  function reportSlicerEmbedError(error: unknown, logPrefix: string) {
    const message = String(error);
    setSlicerStatus(messages.embedFailed(message));
    addMessage(`${logPrefix}: ${message}`);
  }

  function applyOrcaEmbedResult(
    result: Awaited<ReturnType<typeof embedOrcaWindow>>,
    logPrefix: string,
    trackEmbedSession: boolean,
  ) {
    setSlicerStatus(result.message);
    if (trackEmbedSession) {
      setHasOrcaEmbedSession(result.status === "embedded");
    }
    addMessage(`${logPrefix}: ${result.message}`);
  }

  async function showSlicerView() {
    setWorkspaceView("slicer");

    if (!orcaSlicer.enabled) {
      setSlicerStatus(messages.disabled);
      return;
    }

    if (orcaSlicer.integrationMode === "web") {
      setSlicerStatus(null);
      return;
    }

    setSlicerStatus(messages.opening);
    const binaryPath = orcaSlicer.binaryPath.trim();
    if (!binaryPath) {
      setSlicerStatus(messages.binaryMissing);
      return;
    }

    try {
      await waitForNextFrame();
      const bounds = readSlicerViewportBounds();
      if (!bounds) {
        setSlicerStatus(messages.containerUnavailable);
        return;
      }

      const result = await embedOrcaWindow({
        binaryPath,
        modelFilePath: null,
        bounds,
      });
      applyOrcaEmbedResult(result, "slicer", true);
    } catch (error) {
      reportSlicerEmbedError(error, "slicer error");
    }
  }

  async function uploadStlToOrcaWeb(stlPath: string): Promise<void> {
    const webUrl = orcaSlicer.webUrl.trim();
    const response = await fetch(stlPath);
    if (!response.ok) {
      throw new Error(`Failed to read STL file: ${response.statusText}`);
    }
    const blob = await response.blob();

    const formData = new FormData();
    formData.append("file", blob, "model.stl");

    const uploadResponse = await fetch(`${webUrl}/api/upload`, {
      method: "POST",
      body: formData,
    });
    if (!uploadResponse.ok) {
      throw new Error(
        `OrcaSlicer web upload failed (${uploadResponse.status}): ${uploadResponse.statusText}`,
      );
    }
  }

  async function exportToSlicer() {
    await exportToSlicerFromContext({
      hasExportableBody,
      orcaSlicer,
      messages: {
        noExportableBody: messages.noExportableBody,
        disabled: messages.disabled,
        exporting: messages.exporting,
        binaryMissing: messages.binaryMissing,
        containerUnavailable: messages.containerUnavailable,
      },
      setSlicerStatus,
      setWorkspaceView: (view) => setWorkspaceView(view),
      addMessage,
      prepareExportedSlicerStl,
      uploadStlToOrcaWeb,
      waitForNextFrame,
      readSlicerViewportBounds,
      applyOrcaEmbedResult,
      reportSlicerEmbedError,
    });
  }

  function handleWorkspaceDropdownOpenChange(isOpen: boolean) {
    if (!hasOrcaEmbedSession || workspaceViewRef.current !== "slicer") {
      return;
    }
    void setOrcaMapped(!isOpen).catch((error) => {
      addMessage(`slicer map error: ${String(error)}`);
    });
  }

  useEffect(() => {
    if (workspaceView !== "slicer" || !hasOrcaEmbedSession) {
      return;
    }

    let frameId = 0;
    const syncBounds = () => {
      window.cancelAnimationFrame(frameId);
      frameId = window.requestAnimationFrame(() => {
        const bounds = readSlicerViewportBounds();
        if (!bounds) {
          return;
        }
        void resizeOrcaWindow(bounds).catch((error) => {
          addMessage(`slicer resize error: ${String(error)}`);
        });
      });
    };

    const observer = new ResizeObserver(syncBounds);
    if (slicerViewportRef.current) {
      observer.observe(slicerViewportRef.current);
    }
    window.addEventListener("resize", syncBounds);
    syncBounds();

    return () => {
      window.cancelAnimationFrame(frameId);
      window.removeEventListener("resize", syncBounds);
      observer.disconnect();
    };
  }, [workspaceView, hasOrcaEmbedSession, addMessage, slicerViewportRef]);

  return {
    exportToSlicer,
    handleWorkspaceDropdownOpenChange,
    showCadView,
    showCamView,
    showSlicerView,
  };
}
