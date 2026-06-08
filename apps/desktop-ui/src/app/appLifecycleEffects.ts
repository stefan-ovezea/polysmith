import { useEffect } from "react";
import type { Dispatch, MutableRefObject, SetStateAction } from "react";
import { getCurrentWindow } from "@tauri-apps/api/window";

import { loadRecentProjects, type RecentProjectsDocument } from "../lib";
import type { CategoryId } from "../layout";
import type { DocumentState } from "../types";
import type {
  PendingUnsavedAction,
  SavedDocumentBaseline,
} from "./appState";

type CoreStatus = "idle" | "starting" | "connected" | "error" | "stopped";

interface AppLifecycleEffectsContext {
  status: CoreStatus;
  document: DocumentState | null;
  start: () => Promise<void>;
  createDocument: () => Promise<void>;
  addMessage: (message: string) => void;
  isDocumentDirty: boolean;
  windowDocumentTitle: string;
  recentProjectsDocumentRef: MutableRefObject<RecentProjectsDocument>;
  allowAppCloseRef: MutableRefObject<boolean>;
  isDocumentDirtyRef: MutableRefObject<boolean>;
  originVisibilityManuallyChangedRef: MutableRefObject<boolean>;
  setRecentProjectsDocument: Dispatch<
    SetStateAction<RecentProjectsDocument>
  >;
  setCurrentProjectPath: Dispatch<SetStateAction<string | null>>;
  setSavedDocumentBaseline: Dispatch<
    SetStateAction<SavedDocumentBaseline | null>
  >;
  setPendingUnsavedAction: Dispatch<
    SetStateAction<PendingUnsavedAction | null>
  >;
  setHiddenFeatureIds: Dispatch<SetStateAction<Set<string>>>;
  setHiddenCategories: Dispatch<SetStateAction<Set<CategoryId>>>;
}

export function useAppLifecycleEffects({
  status,
  document,
  start,
  createDocument,
  addMessage,
  isDocumentDirty,
  windowDocumentTitle,
  recentProjectsDocumentRef,
  allowAppCloseRef,
  isDocumentDirtyRef,
  originVisibilityManuallyChangedRef,
  setRecentProjectsDocument,
  setCurrentProjectPath,
  setSavedDocumentBaseline,
  setPendingUnsavedAction,
  setHiddenFeatureIds,
  setHiddenCategories,
}: AppLifecycleEffectsContext) {
  isDocumentDirtyRef.current = isDocumentDirty;

  useEffect(() => {
    if (status === "idle" || status === "stopped") {
      void start();
      return;
    }
    if (status === "connected" && document === null) {
      void createDocument();
    }
  }, [status, document, start, createDocument]);

  useEffect(() => {
    if (status !== "stopped") {
      return;
    }
    setCurrentProjectPath(null);
    setSavedDocumentBaseline(null);
    setPendingUnsavedAction(null);
    setHiddenFeatureIds(new Set<string>());
    setHiddenCategories(new Set<CategoryId>());
    originVisibilityManuallyChangedRef.current = false;
  }, [
    status,
    originVisibilityManuallyChangedRef,
    setCurrentProjectPath,
    setSavedDocumentBaseline,
    setPendingUnsavedAction,
    setHiddenFeatureIds,
    setHiddenCategories,
  ]);

  useEffect(() => {
    let canceled = false;
    void loadRecentProjects()
      .then((projectsDocument) => {
        if (!canceled) {
          recentProjectsDocumentRef.current = projectsDocument;
          setRecentProjectsDocument(projectsDocument);
        }
      })
      .catch((error) => {
        addMessage(`recent projects load error: ${String(error)}`);
      });
    return () => {
      canceled = true;
    };
  }, [addMessage, recentProjectsDocumentRef, setRecentProjectsDocument]);

  useEffect(() => {
    let unlisten: (() => void) | null = null;
    let disposed = false;
    void getCurrentWindow()
      .onCloseRequested((event) => {
        if (allowAppCloseRef.current || !isDocumentDirtyRef.current) {
          return;
        }
        event.preventDefault();
        setPendingUnsavedAction({ kind: "quit" });
      })
      .then((nextUnlisten) => {
        if (disposed) {
          nextUnlisten();
          return;
        }
        unlisten = nextUnlisten;
      });
    return () => {
      disposed = true;
      unlisten?.();
    };
  }, [allowAppCloseRef, isDocumentDirtyRef, setPendingUnsavedAction]);

  useEffect(() => {
    void getCurrentWindow()
      .setTitle(windowDocumentTitle)
      .catch((error) => {
        addMessage(`window title error: ${String(error)}`);
      });
  }, [windowDocumentTitle, addMessage]);

  useEffect(() => {
    function handleBeforeUnload(event: BeforeUnloadEvent) {
      if (!isDocumentDirty) {
        return;
      }
      event.preventDefault();
      event.returnValue = "";
    }

    window.addEventListener("beforeunload", handleBeforeUnload);
    return () => {
      window.removeEventListener("beforeunload", handleBeforeUnload);
    };
  }, [isDocumentDirty]);
}
