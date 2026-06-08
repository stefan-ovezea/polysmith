import type { ParameterEntry } from "@/types";
import {
  fuzzyParameterScore,
  parameterTokenAtCursor,
  type ParameterSuggestion,
} from "./draftDimensions";

export function getDimensionParameterSuggestions({
  parameters,
  value,
  cursor,
  isAngleDimension,
}: {
  parameters: ParameterEntry[] | undefined;
  value: string;
  cursor: number;
  isAngleDimension: boolean;
}): ParameterSuggestion[] {
  if (!parameters?.length) {
    return [];
  }
  const token = parameterTokenAtCursor(value, cursor);
  if (!token) {
    return [];
  }
  const normalizedQuery = token.query.toLowerCase();
  if (
    parameters.some(
      (parameter) =>
        !parameter.has_error &&
        parameter.name.toLowerCase() === normalizedQuery,
    )
  ) {
    return [];
  }
  return parameters
    .filter((parameter) => !parameter.has_error)
    .filter((parameter) =>
      isAngleDimension ? parameter.kind === "angle" : parameter.kind !== "angle",
    )
    .map((parameter) => ({
      parameter,
      score: fuzzyParameterScore(token.query, parameter.name),
    }))
    .filter((entry) => entry.score > 0)
    .sort((left, right) => right.score - left.score)
    .slice(0, 6)
    .map(({ parameter }) => ({
      name: parameter.name,
      expression: parameter.expression,
      kind: parameter.kind,
      value: parameter.resolved_value,
    }));
}
