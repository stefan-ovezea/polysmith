import { open, save } from "@tauri-apps/plugin-dialog";
import {
  directoryFromFilePath,
  readLastUsedDirectory,
  writeLastUsedDirectory,
} from "./lastUsedDirectory";

export type DialogTranslate = (key: string) => string;

export interface DocumentDialogContext {
  translate: DialogTranslate;
  documentName?: string | null;
  addMessage: (message: string) => void;
  directory?: string | null;
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

export async function pickGcodeExportPath({
  translate,
  documentName,
  addMessage,
}: DocumentDialogContext) {
  const filePath = await save({
    title: translate("dialogs.exportGcodeTitle"),
    defaultPath: `${makeDefaultExportBaseName(documentName)}.nc`,
    filters: [
      {
        name: translate("dialogs.gcodeFileType"),
        extensions: ["nc", "gcode", "tap", "ngc"],
      },
    ],
  });

  if (filePath === null) {
    addMessage("export canceled");
    return null;
  }

  return filePath;
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

export async function pickDrawingSvgPath({
  translate,
  documentName,
  addMessage,
}: DocumentDialogContext) {
  const filePath = await save({
    title: translate("dialogs.exportDrawingSvgTitle"),
    defaultPath: `${makeDefaultExportBaseName(documentName)}.svg`,
    filters: [
      {
        name: translate("dialogs.svgFileType"),
        extensions: ["svg"],
      },
    ],
  });

  if (filePath === null) {
    addMessage("export canceled");
    return null;
  }

  return filePath;
}

export async function pickDrawingDxfPath({
  translate,
  documentName,
  addMessage,
}: DocumentDialogContext) {
  const filePath = await save({
    title: translate("dialogs.exportDrawingDxfTitle"),
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

export async function pickDrawingPdfPath({
  translate,
  documentName,
  addMessage,
}: DocumentDialogContext) {
  const filePath = await save({
    title: translate("dialogs.exportDrawingPdfTitle"),
    defaultPath: `${makeDefaultExportBaseName(documentName)}.pdf`,
    filters: [
      {
        name: translate("dialogs.pdfFileType"),
        extensions: ["pdf"],
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
  directory,
}: DocumentDialogContext) {
  const baseName = makeDefaultExportBaseName(documentName);
  const dialogDirectory = directory ?? readLastUsedDirectory();
  const filePath = await save({
    title: translate("dialogs.saveDocumentTitle"),
    // A forward slash works on Windows too — the dialog plugin builds
    // the path through PathBuf and splits it into directory + filename.
    defaultPath: dialogDirectory
      ? `${dialogDirectory}/${baseName}.polysmith`
      : `${baseName}.polysmith`,
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

  writeLastUsedDirectory(directoryFromFilePath(filePath));
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
    defaultPath: readLastUsedDirectory() ?? undefined,
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

  writeLastUsedDirectory(directoryFromFilePath(result));
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

export async function pickImportIgesPath({
  translate,
  addMessage,
}: DocumentDialogContext) {
  const result = await open({
    title: translate("dialogs.importIgesTitle"),
    multiple: false,
    directory: false,
    filters: [
      {
        name: translate("dialogs.igesFileType"),
        extensions: ["iges", "igs"],
      },
    ],
  });

  if (result === null || Array.isArray(result)) {
    addMessage("import canceled");
    return null;
  }

  return result;
}

export async function pickExportIgesPath({
  translate,
  documentName,
  addMessage,
}: DocumentDialogContext) {
  const filePath = await save({
    title: translate("dialogs.exportIgesTitle"),
    defaultPath: `${makeDefaultExportBaseName(documentName)}.iges`,
    filters: [
      {
        name: translate("dialogs.igesFileType"),
        extensions: ["iges", "igs"],
      },
    ],
  });

  if (filePath === null) {
    addMessage("export canceled");
    return null;
  }

  return filePath;
}
