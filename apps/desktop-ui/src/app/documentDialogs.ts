import { open, save } from "@tauri-apps/plugin-dialog";

export type DialogTranslate = (key: string) => string;

export interface DocumentDialogContext {
  translate: DialogTranslate;
  documentName?: string | null;
  addMessage: (message: string) => void;
}

export function makeDefaultExportBaseName(name?: string | null) {
  return (
    (name ?? "polysmith-part")
      .trim()
      .toLowerCase()
      .replace(/[^a-z0-9]+/g, "-")
      .replace(/^-+|-+$/g, "") || "polysmith-part"
  );
}

export async function pickExportPath({
  translate,
  documentName,
  addMessage,
}: DocumentDialogContext) {
  const filePath = await save({
    title: translate("dialogs.exportStepTitle"),
    defaultPath: `${makeDefaultExportBaseName(documentName)}.step`,
    filters: [
      {
        name: translate("dialogs.stepFileType"),
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

export async function pickExportStlPath({
  translate,
  documentName,
  addMessage,
}: DocumentDialogContext) {
  const filePath = await save({
    title: translate("dialogs.exportMeshTitle"),
    defaultPath: `${makeDefaultExportBaseName(documentName)}.stl`,
    filters: [
      {
        name: translate("dialogs.stlFileType"),
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

export async function pickExportDxfPath({
  translate,
  documentName,
  addMessage,
}: DocumentDialogContext) {
  const filePath = await save({
    title: translate("dialogs.exportDxfTitle"),
    defaultPath: `${makeDefaultExportBaseName(documentName)}.dxf`,
    filters: [
      {
        name: translate("dialogs.dxfFileType"),
        extensions: ["dxf"],
      },
    ],
  });

  if (filePath === null) {
    addMessage("export canceled");
    return null;
  }

  return filePath;
}

export async function pickSaveDocumentPath({
  translate,
  documentName,
  addMessage,
}: DocumentDialogContext) {
  const filePath = await save({
    title: translate("dialogs.saveDocumentTitle"),
    defaultPath: `${makeDefaultExportBaseName(documentName)}.polysmith`,
    filters: [
      {
        name: translate("dialogs.polysmithDocumentType"),
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

export async function pickLoadDocumentPath({
  translate,
  addMessage,
}: DocumentDialogContext) {
  const result = await open({
    title: translate("dialogs.openDocumentTitle"),
    multiple: false,
    directory: false,
    filters: [
      {
        name: translate("dialogs.polysmithDocumentType"),
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

export async function pickImportStlPath({
  translate,
  addMessage,
}: DocumentDialogContext) {
  const result = await open({
    title: translate("dialogs.importStlTitle"),
    multiple: false,
    directory: false,
    filters: [
      {
        name: translate("dialogs.stlFileType"),
        extensions: ["stl"],
      },
    ],
  });

  if (result === null || Array.isArray(result)) {
    addMessage("import canceled");
    return null;
  }

  return result;
}

export async function pickImportDxfPath({
  translate,
  addMessage,
}: DocumentDialogContext) {
  const result = await open({
    title: translate("dialogs.importDxfTitle"),
    multiple: false,
    directory: false,
    filters: [
      {
        name: translate("dialogs.dxfFileType"),
        extensions: ["dxf"],
      },
    ],
  });

  if (result === null || Array.isArray(result)) {
    addMessage("import canceled");
    return null;
  }

  return result;
}

export async function pickImportStepPath({
  translate,
  addMessage,
}: DocumentDialogContext) {
  const result = await open({
    title: translate("dialogs.importStepTitle"),
    multiple: false,
    directory: false,
    filters: [
      {
        name: translate("dialogs.stepFileType"),
        extensions: ["step", "stp"],
      },
    ],
  });

  if (result === null || Array.isArray(result)) {
    addMessage("import canceled");
    return null;
  }

  return result;
}
