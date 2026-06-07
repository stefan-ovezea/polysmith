export function normalizeNumberInputValue(value: string) {
  if (value === "") {
    return value;
  }
  const sign = value.startsWith("-") ? "-" : "";
  const unsigned = sign ? value.slice(1) : value;
  if (unsigned.startsWith("0.") || unsigned === "0") {
    return value;
  }
  const normalized = unsigned.replace(/^0+(?=\d)/, "");
  return `${sign}${normalized || "0"}`;
}

export function readNumberInputValue(input: HTMLInputElement) {
  const normalized = normalizeNumberInputValue(input.value);
  if (normalized !== input.value) {
    input.value = normalized;
  }
  return Number(normalized);
}
