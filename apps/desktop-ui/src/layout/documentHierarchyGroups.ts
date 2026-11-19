// Grouping rules for the Browser tree.
//
// An imported file arrives as MANY features — one per solid — and every one of
// them carries the same `operation_id` (the importer stamps it, the timeline
// folds them into one row, and `delete_feature` removes them as one undo step).
// The Browser must present the same entity: one node per import, its bodies as
// children, and none of those bodies loose in the Bodies category. Fusion and
// SolidCraft both do this, and it is the difference between a usable tree and a
// thousand-row list.

const IMPORT_BODY_KINDS = new Set(["step_import", "iges_import"]);

const IMPORT_FILE_EXTENSIONS = [".step", ".stp", ".iges", ".igs"];

export interface HierarchyImportBody {
  feature_id: string;
  kind: string;
  name: string;
  operation_id?: string;
  parameters_summary?: string;
  suppressed?: boolean;
  dependency_broken?: boolean;
  dependency_warning?: string;
}

export interface ImportBodyGroup<F extends HierarchyImportBody> {
  /** Shared operation id, or the feature id for a standalone import. */
  operationId: string;
  members: F[];
  /** The member the node acts on (timeline row, delete, rename target). */
  primary: F;
  /**
   * The imported file's name, or null when the summary does not name one — the
   * caller falls back to a translated generic label.
   */
  label: string | null;
}

export function isImportedBody(feature: { kind: string }): boolean {
  return IMPORT_BODY_KINDS.has(feature.kind);
}

// Mirrors the core's ComponentNode: one occurrence of a part in an imported
// assembly ("U1:1"), which is the file's name for that PLACEMENT.
export interface HierarchyComponent {
  component_id: string;
  name: string;
  parent_id: string;
  body_ids: string[];
}

export interface ComponentTreeNode<C extends HierarchyComponent> {
  component: C;
  /** Bodies of this occurrence that belong to the import being listed. */
  bodyIds: string[];
  children: ComponentTreeNode<C>[];
}

/**
 * The component occurrences of one import, nested.
 *
 * `memberIds` scopes the tree to a single import: a document can hold several
 * imports, each with its own hierarchy, and a component belongs to the import
 * that owns the bodies it places. Occurrences left with no bodies and no
 * surviving child are dropped, so a tree never shows a folder that leads to
 * nothing after its bodies were deleted.
 */
export function componentForest<C extends HierarchyComponent>(
  components: readonly C[],
  memberIds: ReadonlySet<string>,
): ComponentTreeNode<C>[] {
  // Only what this import created, keeping the core's (pre-)order.
  const owned: ComponentTreeNode<C>[] = [];
  const byId = new Map<string, ComponentTreeNode<C>>();
  const ids = new Set(components.map((component) => component.component_id));
  for (const component of components) {
    const bodyIds = component.body_ids.filter((id) => memberIds.has(id));
    const node: ComponentTreeNode<C> = {
      component,
      bodyIds,
      children: [],
    };
    owned.push(node);
    byId.set(component.component_id, node);
  }

  const roots: ComponentTreeNode<C>[] = [];
  for (const node of owned) {
    const parentId = node.component.parent_id;
    // A parent that is not part of this list (another import's tree, or a
    // pruned ancestor) makes the node a root rather than losing it.
    const parent = parentId.length > 0 ? byId.get(parentId) : undefined;
    if (parent && parent !== node && ids.has(parentId)) {
      parent.children.push(node);
    } else {
      roots.push(node);
    }
  }

  const prune = (node: ComponentTreeNode<C>): ComponentTreeNode<C> | null => {
    const children = node.children
      .map(prune)
      .filter((child): child is ComponentTreeNode<C> => child !== null);
    if (node.bodyIds.length === 0 && children.length === 0) {
      return null;
    }
    return { component: node.component, bodyIds: node.bodyIds, children };
  };
  return roots
    .map(prune)
    .filter((node): node is ComponentTreeNode<C> => node !== null);
}

/** Every body of a component and of everything nested inside it. */
export function componentBodyIds<C extends HierarchyComponent>(
  node: ComponentTreeNode<C>,
): string[] {
  const ids = [...node.bodyIds];
  for (const child of node.children) {
    ids.push(...componentBodyIds(child));
  }
  return ids;
}

/**
 * One import's tree, plus the set of bodies it places.
 *
 * The leftovers are what the panel lists directly under the import: a surface
 * body the file keeps outside the product structure, or a part whose occurrence
 * the reader could not match. Both must stay reachable.
 */
export function componentHierarchy<C extends HierarchyComponent>(
  components: readonly C[],
  memberIds: readonly string[],
): { forest: ComponentTreeNode<C>[]; placed: ReadonlySet<string> } {
  const forest = componentForest(components, new Set(memberIds));
  return { forest, placed: new Set(forest.flatMap(componentBodyIds)) };
}

/**
 * The occurrences that hold `bodyId`, outermost first, ending with the
 * occurrence that owns it directly. Empty when no component places the body.
 *
 * A row for a body can only be shown once its ancestors are open, which is what
 * keeps "Find in Timeline" working for a part buried six levels down in a board.
 */
export function componentAncestors<C extends HierarchyComponent>(
  components: readonly C[],
  bodyId: string,
): string[] {
  const byId = new Map(components.map((component) => [component.component_id, component]));
  for (const component of components) {
    if (!component.body_ids.includes(bodyId)) {
      continue;
    }
    const chain = [component.component_id];
    const seen = new Set(chain);
    let parentId = component.parent_id;
    while (parentId.length > 0 && !seen.has(parentId)) {
      const parent = byId.get(parentId);
      if (!parent) {
        break;
      }
      seen.add(parentId);
      chain.push(parentId);
      parentId = parent.parent_id;
    }
    return chain.reverse();
  }
  return [];
}

/**
 * The file an import came from, for the tree label.
 *
 * The importer writes "<file> · <n> faces" as the feature summary and the payload
 * does not expose the path to the UI, so the summary is read rather than adding a
 * protocol field just for a label.
 */
export function importGroupFileLabel(
  members: ReadonlyArray<{ parameters_summary?: string }>,
): string | null {
  const summary = members[0]?.parameters_summary ?? "";
  const separator = summary.indexOf("·");
  const file = (separator > 0 ? summary.slice(0, separator) : summary).trim();
  if (file.length === 0) {
    return null;
  }
  const lower = file.toLowerCase();
  return IMPORT_FILE_EXTENSIONS.some((extension) => lower.endsWith(extension))
    ? file
    : null;
}

/** One group per import operation, in first-appearance order. */
export function importBodyGroups<F extends HierarchyImportBody>(
  features: readonly F[],
): ImportBodyGroup<F>[] {
  const groups = new Map<string, F[]>();
  for (const feature of features) {
    if (!isImportedBody(feature)) {
      continue;
    }
    const key = feature.operation_id || feature.feature_id;
    const members = groups.get(key);
    if (members) {
      members.push(feature);
    } else {
      groups.set(key, [feature]);
    }
  }
  return [...groups.entries()].map(([operationId, members]) => ({
    operationId,
    members,
    primary: members[0],
    label: importGroupFileLabel(members),
  }));
}
