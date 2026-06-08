import { Dropdown, findHoleStandard, type HoleStandardEntry } from "../lib";
import type { HoleStandard } from "../types";

interface HoleStandardSizeDropdownProps {
  className?: string;
  disabled: boolean;
  label: string;
  size: string;
  standard: HoleStandard;
  standards: HoleStandardEntry[];
  onChange: (size: string) => void;
}

export function HoleStandardSizeDropdown({
  className,
  disabled,
  label,
  size,
  standard,
  standards,
  onChange,
}: HoleStandardSizeDropdownProps) {
  return (
    <Dropdown<string>
      label={label}
      className={className}
      value={findHoleStandard(standard, size)?.id ?? standards[0]?.id ?? ""}
      disabled={disabled || standards.length === 0}
      options={standards.map((entry) => ({
        value: entry.id,
        label: entry.label,
      }))}
      onChange={onChange}
    />
  );
}
