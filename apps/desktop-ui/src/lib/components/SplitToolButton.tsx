import {
  useId,
  useRef,
  useState,
  type ReactNode,
} from "react";
import { useDismissableLayer } from "./useDismissableLayer";

interface SplitToolOption<TValue extends string> {
  value: TValue;
  label: string;
  icon?: ReactNode;
}

interface SplitToolButtonProps<TValue extends string> {
  /** All variants for this tool. The first entry is the default. */
  options: [SplitToolOption<TValue>, ...SplitToolOption<TValue>[]];
  /** Currently selected variant value. */
  value: TValue;
  /** Called with the new variant value when the user picks from the dropdown. */
  onChange: (value: TValue) => void;
  /** Called when the user clicks the main (icon) area. */
  onPrimaryAction: () => void;
  /** Whether the tool is active / selected. */
  isActive: boolean;
  /** Whether the whole button is disabled. */
  disabled?: boolean;
  /** Tooltip string for the primary area. */
  tooltip?: string;
  /** Icon element rendered in the primary area. */
  children: ReactNode;
  /** Aria label. */
  ariaLabel?: string;
}

export function SplitToolButton<TValue extends string>({
  options,
  value,
  onChange,
  onPrimaryAction,
  isActive,
  disabled = false,
  tooltip,
  children,
  ariaLabel,
}: SplitToolButtonProps<TValue>) {
  const [isOpen, setIsOpen] = useState(false);
  const rootRef = useRef<HTMLDivElement | null>(null);
  const menuId = useId();
  const selectedLabel =
    options.find((o) => o.value === value)?.label ??
    options[0].label;

  useDismissableLayer(isOpen, rootRef, () => setIsOpen(false));

  return (
    <div
      ref={rootRef}
      className="cad-split-tool-root"
      aria-disabled={disabled ? "true" : undefined}
    >
      <button
        type="button"
        className={
          isActive
            ? "cad-split-tool-primary cad-split-tool-primary-active"
            : "cad-split-tool-primary"
        }
        data-tooltip={tooltip}
        aria-label={ariaLabel}
        disabled={disabled}
        onClick={onPrimaryAction}
      >
        {children}
      </button>
      <button
        type="button"
        className={
          isActive
            ? "cad-split-tool-chevron cad-split-tool-chevron-active"
            : "cad-split-tool-chevron"
        }
        aria-haspopup="listbox"
        aria-expanded={isOpen}
        aria-controls={menuId}
        aria-label={`${selectedLabel} — select variant`}
        disabled={disabled}
        onClick={() => setIsOpen((current) => !current)}
        onKeyDown={(event) => {
          if (event.key === "ArrowDown" || event.key === "ArrowUp") {
            event.preventDefault();
            setIsOpen(true);
          }
        }}
      >
        <svg
          viewBox="0 0 10 6"
          className="cad-split-tool-chevron-icon"
          fill="none"
          stroke="currentColor"
          strokeWidth="1.5"
          strokeLinecap="round"
          strokeLinejoin="round"
          aria-hidden="true"
        >
          <path d="M1 1l4 4 4-4" />
        </svg>
      </button>

      {isOpen ? (
        <div
          id={menuId}
          role="listbox"
          aria-label={ariaLabel}
          className="cad-split-tool-menu cad-scrollbar"
        >
          {options.map((option) => {
            const isSelected = option.value === value;
            return (
              <button
                key={option.value}
                type="button"
                role="option"
                aria-selected={isSelected}
                className={
                  isSelected
                    ? "cad-split-tool-option cad-split-tool-option-selected"
                    : "cad-split-tool-option"
                }
                onClick={() => {
                  onChange(option.value);
                  setIsOpen(false);
                }}
              >
                {option.icon ? (
                  <span className="cad-split-tool-option-icon">
                    {option.icon}
                  </span>
                ) : null}
                <span>{option.label}</span>
              </button>
            );
          })}
        </div>
      ) : null}
    </div>
  );
}
