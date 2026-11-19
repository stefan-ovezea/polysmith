import { create } from "zustand";
import type {
  CamGenerationResultEvent,
  SketchEntitiesChangedEvent,
  FeatureChangedEvent,
  SketchDiagnosticsEvent,
  SketchProfilesEvent,
  CoreMessage,
  DocumentState,
  DocumentExportResult,
  LogEntry,
  SessionState,
  ViewportState,
} from "../types/ipc";
import {
  mergeViewportMeshes,
  noteDocumentId,
  resetViewportMeshCache,
} from "../lib/viewportMeshCache";
import {
  getDocumentFromMessage,
  getDocumentExportFromMessage,
  getErrorFromMessage,
  getViewportFromMessage,
} from "../lib/ipcProtocol";
import {
  shouldFailPendingCommand,
  subscribeCoreActivity,
} from "../lib/cadCoreClient";
import { useToastStore } from "./toastStore";
import { appendLogWithinWindow } from "../lib/logWindow";

// ── Atomic commit pairing ─────────────────────────────────────────────
// A commit can carry `include_viewport` (protocol/schema/commands.schema.json):
// the core then follows its reply with a `viewport_state` bearing the SAME id.
// We hold the document message and apply it together with that viewport in ONE
// set(), so React never renders a document update against the stale viewport —
// which is the "old position flashes on commit" bug. The geometry arrives in
// the same update as the document.
const atomicCommandIds = new Set<string>();
const bufferedAtomicDocuments = new Map<string, CoreMessage>();

/** Register a command id whose reply should be applied atomically. */
export function markAtomicCommand(id: string): void {
  atomicCommandIds.add(id);
}

// Kinds of long-running operation the busy overlay knows how to describe.
// The human label lives in i18n under `busy.<kind>`, so this stays a plain
// list of ids shared by the store, the hook and the overlay.
export type BusyOperationKind =
  | "import_step"
  | "import_iges"
  | "import_stl"
  | "import_dxf"
  | "import_pcb"
  | "load_document";

export interface BusyOperation {
  kind: BusyOperationKind;
  fileName?: string;
  startedAt: number;
  // Last stage line the core reported for this operation, if any.
  detail: string;
  // Core log sources whose messages become the stage line. An import logs
  // "Read 1091 solid(s): 1091 named, 359 coloured" part way through, which is
  // the difference between a frozen window and visible progress.
  sources: string[];
}

interface CadCoreStoreState {
  status: "idle" | "starting" | "connected" | "error" | "stopped";
  messages: string[];
  logs: LogEntry[];
  document: DocumentState | null;
  session: SessionState | null;
  viewport: ViewportState | null;
  lastExport: DocumentExportResult | null;
  lastEvent: CoreMessage | null;
  // Latest cam_generation_result payload (generate-only core event).
  // The CAM result popup reads this and dismisses it on close.
  generationResult: CamGenerationResultEvent["payload"] | null;
  // S5: loose-end diagnostics for the sketch the UI last asked about. Derived
  // data with no home in the document, so it lives beside it.
  sketchDiagnostics: SketchDiagnosticsEvent["payload"] | null;
  // The long-running operation the UI is waiting on, if any. A STEP import of
  // a PCB takes ~16 s and a project load of a 24k-face board several more;
  // without this the window looks frozen for the whole time.
  busyOperation: BusyOperation | null;
  setStatus: (status: CadCoreStoreState["status"]) => void;
  handleCoreStopped: () => void;
  addMessage: (message: string) => void;
  addLogEntry: (entry: LogEntry) => void;
  clearLogs: () => void;
  dismissGenerationResult: () => void;
  beginBusyOperation: (operation: {
    kind: BusyOperationKind;
    fileName?: string;
    // Core log sources whose messages become the overlay's stage line.
    sources?: string[];
  }) => void;
  updateBusyOperation: (detail: string) => void;
  endBusyOperation: () => void;
  setSketchDiagnostics: (
    diagnostics: SketchDiagnosticsEvent["payload"] | null,
  ) => void;
  handleCoreMessage: (message: CoreMessage) => void;
}

// Wait until the store receives a new `document` snapshot that satisfies a
// predicate. The IPC bridge is fire-and-forget (commands write to cad_core
// stdin and return synchronously, while responses arrive later through the
// `cad-core-event` Tauri event). Callers that need to read post-command
// state from the store must wait for the next event instead of reading the
// store immediately after `await sendCoreCommand(...)`.
//
// The wait fails when the CORE goes quiet, not when a fixed wall clock passes:
// the core handles commands strictly in arrival order, so a document or viewport
// produced behind a long operation (a ~12 s STEP import, a ~6 s viewport
// rebuild) legitimately arrives well after a flat 4 s deadline — which is how a
// healthy core produced "awaitViewportChange: timed out" as an unhandled
// rejection.
const AWAIT_CORE_IDLE_TIMEOUT_MS = 10_000;
// The wait is for a state that follows a command that has ALREADY been sent, so
// a hard cap keeps a genuinely lost update from sitting there for minutes even
// if the core keeps narrating unrelated work.
const AWAIT_CORE_ABSOLUTE_TIMEOUT_MS = 30_000;

// Shared deadline machinery for the await* helpers below. Returns a cleanup
// function; `onTimeout` fires when the core has been silent for the idle window,
// or when the absolute cap is reached.
function armCoreQuietTimeout(onTimeout: () => void): () => void {
  const startedAt = Date.now();
  let lastActivityAt = startedAt;
  const unsubscribe = subscribeCoreActivity(() => {
    lastActivityAt = Date.now();
  });
  const timer = window.setInterval(() => {
    if (
      shouldFailPendingCommand({
        now: Date.now(),
        sentAt: startedAt,
        lastActivityAt,
        idleTimeoutMs: AWAIT_CORE_IDLE_TIMEOUT_MS,
        absoluteTimeoutMs: AWAIT_CORE_ABSOLUTE_TIMEOUT_MS,
      })
    ) {
      cleanup();
      onTimeout();
    }
  }, 500);
  const cleanup = () => {
    window.clearInterval(timer);
    unsubscribe();
  };
  return cleanup;
}

export function awaitDocumentChange(
  predicate: (next: DocumentState, previous: DocumentState | null) => boolean,
): Promise<DocumentState> {
  return new Promise((resolve, reject) => {
    const initial = useCadCoreStore.getState().document;
    const cleanup = armCoreQuietTimeout(() => {
      unsubscribe();
      reject(new Error("awaitDocumentChange: timed out"));
    });
    const unsubscribe = useCadCoreStore.subscribe((state) => {
      const doc = state.document;
      if (!doc || doc === initial) {
        return;
      }
      if (predicate(doc, initial)) {
        cleanup();
        unsubscribe();
        resolve(doc);
      }
    });
  });
}

// The viewport twin of awaitDocumentChange — waits for a fresh
// viewport_state (the drawing auto-fit needs the sheet view bounds,
// which live only in the viewport payload, not the document).
export function awaitViewportChange(
  predicate: (next: ViewportState, previous: ViewportState | null) => boolean,
): Promise<ViewportState> {
  return new Promise((resolve, reject) => {
    const initial = useCadCoreStore.getState().viewport;
    const cleanup = armCoreQuietTimeout(() => {
      unsubscribe();
      reject(new Error("awaitViewportChange: timed out"));
    });
    const unsubscribe = useCadCoreStore.subscribe((state) => {
      const vp = state.viewport;
      if (!vp || vp === initial) {
        return;
      }
      if (predicate(vp, initial)) {
        cleanup();
        unsubscribe();
        resolve(vp);
      }
    });
  });
}

export function awaitDocumentExport(
  predicate: (next: DocumentExportResult) => boolean,
): Promise<DocumentExportResult> {
  return new Promise((resolve, reject) => {
    const initial = useCadCoreStore.getState().lastExport;
    const cleanup = armCoreQuietTimeout(() => {
      unsubscribe();
      reject(new Error("awaitDocumentExport: timed out"));
    });
    const unsubscribe = useCadCoreStore.subscribe((state) => {
      const exportResult = state.lastExport;
      if (!exportResult || exportResult === initial) {
        return;
      }
      if (predicate(exportResult)) {
        cleanup();
        unsubscribe();
        resolve(exportResult);
      }
    });
  });
}

export function awaitDocumentSaved(
  predicate: (filePath: string) => boolean,
): Promise<string> {
  return new Promise((resolve, reject) => {
    const initial = useCadCoreStore.getState().lastEvent;
    const cleanup = armCoreQuietTimeout(() => {
      unsubscribe();
      reject(new Error("awaitDocumentSaved: timed out"));
    });
    const unsubscribe = useCadCoreStore.subscribe((state) => {
      const event = state.lastEvent;
      if (!event || event === initial || event.type !== "document_saved") {
        return;
      }
      const filePath = event.payload.file_path;
      if (predicate(filePath)) {
        cleanup();
        unsubscribe();
        resolve(filePath);
      }
    });
  });
}

export const useCadCoreStore = create<CadCoreStoreState>((set) => ({
  status: "idle",
  messages: [],
  logs: [],
  document: null,
  session: null,
  viewport: null,
  lastExport: null,
  lastEvent: null,
  generationResult: null,
  sketchDiagnostics: null,
  busyOperation: null,
  setStatus: (status) => set({ status }),
  handleCoreStopped: () =>
    set({
      status: "stopped",
      document: null,
      session: null,
      viewport: null,
      lastExport: null,
      lastEvent: null,
      generationResult: null,
      sketchDiagnostics: null,
      // A command interrupted by the core dying will never answer: drop the
      // overlay instead of leaving it spinning forever.
      busyOperation: null,
    }),
  addMessage: (message) =>
    set((state) => ({ messages: [...state.messages, message].slice(-500) })),
  addLogEntry: (entry) =>
    set((state) => {
      // The core emits every diagnostic twice — once as a structured
      // log event and once on the stderr bridge.  Drop consecutive
      // duplicates so a busy generator doesn't flood the Logs panel.
      const previous = state.logs[state.logs.length - 1];
      if (
        previous &&
        previous.source === entry.source &&
        previous.message === entry.message
      ) {
        return state;
      }
      return { logs: appendLogWithinWindow(state.logs, entry) };
    }),
  clearLogs: () => set({ logs: [] }),
  dismissGenerationResult: () => set({ generationResult: null }),
  beginBusyOperation: ({ kind, fileName, sources }) =>
    set({
      busyOperation: {
        kind,
        fileName,
        startedAt: Date.now(),
        detail: "",
        sources: sources ?? [],
      },
    }),
  updateBusyOperation: (detail) =>
    set((state) =>
      state.busyOperation
        ? { busyOperation: { ...state.busyOperation, detail } }
        : {},
    ),
  endBusyOperation: () => set({ busyOperation: null }),
  setSketchDiagnostics: (sketchDiagnostics) => set({ sketchDiagnostics }),
  handleCoreMessage: (message) =>
    set((state) => {
      // Atomic commit pairing: hold the document until its viewport arrives.
      const atomicId =
        message.id !== undefined && atomicCommandIds.has(message.id)
          ? message.id
          : null;
      if (
        atomicId !== null &&
        (message.type === "document_state" ||
          message.type === "sketch_entities_changed" ||
          message.type === "feature_changed")
      ) {
        bufferedAtomicDocuments.set(atomicId, message);
        return {
          ...state,
          lastEvent: message,
          status: getStatusFromMessage(message, state.status),
          messages: [...state.messages, `event: ${message.type}`].slice(-500),
        } as CadCoreStoreState;
      }

      const nextState: Partial<CadCoreStoreState> = {
        lastEvent: message,
        status: getStatusFromMessage(message, state.status),
      };

      if (message.type === "sketch_diagnostics") {
        nextState.sketchDiagnostics = message.payload;
      }

      const document = getDocumentFromMessage(message);
      if (document) {
        // Detected profiles are derived and no longer travel on the event
        // stream (see serialization.h): the core ships them when a sketch is
        // opened or loaded, and omits them from every later event, so keep the
        // set the client already has for a feature whose live count still
        // matches. The region list, source labels and extrude defaults read
        // them synchronously, exactly as before.
        nextState.document = keepKnownProfiles(document, state.document);
      }

      // Selection-only reply: the core no longer re-sends the whole document for
      // a click (that was 1,219 KB on a dense sketch).  Patch exactly the fields
      // the event carries; everything else in the document is unchanged.
      if (message.type === "selection_changed" && state.document) {
        const selection = message.payload;
        nextState.document = {
          ...state.document,
          revision: selection.revision,
          selected_feature_id: selection.selected_feature_id ?? null,
          selected_reference_id: selection.selected_reference_id ?? null,
          selected_face_id: selection.selected_face_id ?? null,
          selected_edge_ids: selection.selected_edge_ids,
          selected_vertex_ids: selection.selected_vertex_ids,
          selected_sketch_vertex_id: selection.selected_sketch_vertex_id ?? null,
          selected_sketch_entity_id: selection.selected_sketch_entity_id ?? null,
          selected_sketch_vertex_ids: selection.selected_sketch_vertex_ids,
          selected_sketch_entity_ids: selection.selected_sketch_entity_ids,
          selected_sketch_dimension_id:
            selection.selected_sketch_dimension_id ?? null,
          selected_sketch_text_id: selection.selected_sketch_text_id ?? null,
          selected_sketch_profile_id:
            selection.selected_sketch_profile_id ?? null,
          selected_sketch_profile_ids: selection.selected_sketch_profile_ids,
        } as typeof state.document;
      }

      // Incremental sketch edit (phase D1): patch only what the core touched.
      // A stale revision or an unknown feature leaves the document alone; the
      // next full state corrects it, so a missed patch can never be silent.
      if (message.type === "sketch_entities_changed" && state.document) {
        nextState.document = applySketchEntitiesChanged(
          state.document,
          message.payload,
        );
      }

      // Incremental feature edit (phase D2): patch one feature's stored
      // parameters by id. Same stale-revision safety net as D1.
      if (message.type === "feature_changed" && state.document) {
        nextState.document = applyFeatureChanged(
          state.document,
          message.payload,
        );
      }

      // On-demand profile geometry (phase E1): merge the fetched profile
      // regions into the sketch feature that owns them, so the viewport can
      // render the filled surfaces from the document.
      if (message.type === "sketch_profiles" && state.document) {
        nextState.document = applySketchProfiles(state.document, message.payload);
      }

      if (message.type === "session_state") {
        nextState.session = message.payload;
      }

      // A new document invalidates every cached body id, and a LOAD of another
      // document has to reset too: ids and revisions are document-local, so a
      // reload used to reuse the previous document's bodies.
      if (message.type === "document_created") {
        resetViewportMeshCache();
      }
      const messageDocumentId = (message.payload as { document_id?: string })
        .document_id;
      if (typeof messageDocumentId === "string") {
        noteDocumentId(messageDocumentId);
      }
      const viewport = getViewportFromMessage(message);
      if (viewport) {
        // Mesh delta: `viewport.meshes` may hold only the bodies whose geometry
        // changed, so merge it with the cache into a complete snapshot before
        // anything downstream sees it.
        const mergedViewport = mergeViewportMeshes(viewport);
        nextState.viewport = mergedViewport;
        // Atomic commit pairing: merge the document we held for this id into
        // this same set(), so the document and its geometry land together.
        if (atomicId !== null) {
          const buffered = bufferedAtomicDocuments.get(atomicId);
          bufferedAtomicDocuments.delete(atomicId);
          atomicCommandIds.delete(atomicId);
          if (buffered) {
            if (buffered.type === "sketch_entities_changed" && state.document) {
              nextState.document = applySketchEntitiesChanged(
                state.document,
                buffered.payload,
              );
            } else if (buffered.type === "feature_changed" && state.document) {
              nextState.document = applyFeatureChanged(
                state.document,
                buffered.payload,
              );
            } else {
              const doc = getDocumentFromMessage(buffered);
              if (doc) {
                nextState.document = keepKnownProfiles(doc, state.document);
              }
            }
          }
        }
      }

      const documentExport = getDocumentExportFromMessage(message);
      if (documentExport) {
        nextState.lastExport = documentExport;
      }

      if (message.type === "log") {
        nextState.logs = appendLogWithinWindow(state.logs, message.payload);
        // Let the core's own stage lines drive the busy overlay: an import
        // that takes 16 s is fine, an import that says nothing for 16 s is
        // indistinguishable from a crash.
        const busy = nextState.busyOperation;
        if (
          busy &&
          busy.sources.includes(message.payload.source) &&
          message.payload.message !== busy.detail
        ) {
          nextState.busyOperation = {
            ...busy,
            detail: message.payload.message,
          };
        }
      }

      if (message.type === "cam_generation_result") {
        // A repeated Generate replaces the previous result — the popup
        // always shows the most recent outcome.
        nextState.generationResult = message.payload;
      }

      const error = getErrorFromMessage(message);
      if (error) {
        // D6: undo/redo on an empty stack is a designed no-op, not a
        // failure — the core answers with EMPTY_UNDO_STACK /
        // EMPTY_REDO_STACK so awaited callers can observe the result,
        // but it must not pop an error toast (M8).
        const isUndoStackEmpty =
          error.payload.code === "EMPTY_UNDO_STACK" ||
          error.payload.code === "EMPTY_REDO_STACK";
        if (!isUndoStackEmpty) {
          useToastStore
            .getState()
            .pushToast("error", error.payload.message);
        }
      }
      const renderedMessage =
        message.type === "error"
          ? `error: ${error?.payload.code} - ${error?.payload.message}`
          : message.type === "log"
            ? `log: ${message.payload.level} - ${message.payload.message}`
          : message.type === "document_exported"
            ? `event: document_exported - ${message.payload.file_path}`
            : `event: ${message.type}`;

      return {
        ...nextState,
        // Capped like logs — an unbounded array grows with every core
        // event and makes every store update slower over a session.
        messages: [...state.messages, renderedMessage].slice(-500),
      } as CadCoreStoreState;
    }),
}));

function getStatusFromMessage(
  message: CoreMessage,
  prevStatus: CadCoreStoreState["status"],
): CadCoreStoreState["status"] {
  if (message.type === "error") {
    return "error";
  }
  if (
    message.type === "hello" ||
    message.type === "pong" ||
    message.type === "document_created" ||
    message.type === "document_state" ||
    message.type === "viewport_state" ||
    message.type === "document_exported" ||
    message.type === "document_saved" ||
    message.type === "log"
  ) {
    return "connected";
  }
  return prevStatus;
}

/**
 * Carry the detected profiles forward across an event that omitted them.
 *
 * profiles is a derived cache: the core ships it when a sketch is opened or
 * loaded and omits it from every later event (see serialization.h), which is
 * most of an edit's payload on a dense sketch. Every consumer here -- region
 * list, source labels, extrude defaults -- reads the array synchronously, so we
 * keep the last set the core sent for a feature whose live profile_count still
 * matches it. When the count moves, the set is dropped rather than kept stale,
 * and get_sketch_profiles can refetch it.
 */
function keepKnownProfiles(
  incoming: DocumentState,
  previous: DocumentState | null,
): DocumentState {
  if (!previous) {
    return incoming;
  }
  const known = new Map(
    (previous.feature_history ?? [])
      .filter((feature) => feature.kind === 'sketch' && feature.sketch_parameters)
      .map((feature) => [feature.feature_id, feature.sketch_parameters!]),
  );
  let patched = false;
  for (const feature of incoming.feature_history ?? []) {
    const params = feature.sketch_parameters;
    if (feature.kind !== 'sketch' || !params) {
      continue;
    }
    if ((params.profiles?.length ?? 0) > 0) {
      continue;
    }
    const old = known.get(feature.feature_id);
    const oldProfiles = old?.profiles ?? [];
    if (!old || oldProfiles.length === 0) {
      continue;
    }
    if ((params.profile_count ?? 0) !== oldProfiles.length) {
      continue;
    }
    params.profiles = oldProfiles;
    patched = true;
  }
  return patched ? { ...incoming } : incoming;
}


/** Patch a `sketch_entities_changed` event into the document copy. */
function applySketchEntitiesChanged(
  document: DocumentState,
  change: SketchEntitiesChangedEvent["payload"],
): DocumentState {
  const feature = (document.feature_history ?? []).find(
    (entry) => entry.feature_id === change.feature_id,
  );
  const params = feature?.sketch_parameters;
  if (!feature || !params || change.revision <= document.revision) {
    return document;
  }
  const patchById = (
    current: any[] | undefined,
    incoming: Array<Record<string, unknown>> | undefined,
    idKey: string,
  ): any[] | undefined => {
    if (!incoming?.length) {
      return current;
    }
    const byId = new Map((current ?? []).map((entry) => [entry[idKey], entry]));
    for (const entry of incoming) {
      const existing = byId.get(entry[idKey]);
      byId.set(entry[idKey], existing ? { ...existing, ...entry } : entry);
    }
    return [...byId.values()];
  };
  const removed = new Set(change.removed ?? []);
  const lines = patchById(
    params.lines as any[],
    change.lines,
    "line_id",
  )?.filter((line) => !removed.has(line.line_id as string)) as
    | typeof params.lines
    | undefined;
  const vertices = patchById(
    params.vertices as any[],
    change.vertices,
    "vertex_id",
  )?.filter((vertex) => !removed.has(vertex.vertex_id as string)) as
    | typeof params.vertices
    | undefined;
  return {
    ...document,
    revision: change.revision,
    feature_history: (document.feature_history ?? []).map((entry) =>
      entry.feature_id === change.feature_id
        ? {
            ...entry,
            sketch_parameters: {
              ...params,
              lines: lines ?? params.lines,
              vertices: vertices ?? params.vertices,
              profile_count: change.derived?.profile_count ?? params.profile_count,
              vertex_count: change.derived?.vertex_count ?? params.vertex_count,
            },
          }
        : entry,
    ),
  };
}

/** Patch a `feature_changed` event into the document copy (phase D2). */
function applyFeatureChanged(
  document: DocumentState,
  change: FeatureChangedEvent["payload"],
): DocumentState {
  const removed = new Set(change.removed_feature_ids ?? []);
  const history = (document.feature_history ?? []).filter(
    (entry) => !removed.has(entry.feature_id),
  );
  const index = history.findIndex(
    (entry) => entry.feature_id === change.feature_id,
  );
  if (index < 0) {
    // Unknown feature: don't invent state. Leave the document alone; the next
    // full document_state (or a revision-gap refetch) resyncs it.
    return document;
  }
  history[index] = change.feature;
  return {
    ...document,
    revision: change.revision,
    feature_history: history,
  };
}

/** Merge an on-demand `sketch_profiles` fetch into the owning sketch feature. */
function applySketchProfiles(
  document: DocumentState,
  change: SketchProfilesEvent["payload"],
): DocumentState {
  return {
    ...document,
    feature_history: (document.feature_history ?? []).map((entry) =>
      entry.feature_id === change.feature_id
        ? {
            ...entry,
            sketch_parameters: entry.sketch_parameters
              ? {
                  ...entry.sketch_parameters,
                  profiles: change.profiles,
                  profile_count: change.profile_count,
                }
              : entry.sketch_parameters,
          }
        : entry,
    ),
  };
}
