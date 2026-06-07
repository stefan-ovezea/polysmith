import {
  useEffect,
  useId,
  useRef,
  useState,
  type ReactNode,
} from "react";
import { useDismissableLayer } from "./useDismissableLayer";

export interface DropdownOption<TValue extends string> {
  value: TValue;
  label: ReactNode;
  disabled?: boolean;
}

interface DropdownProps<TValue extends string> {
  value: TValue;
  options: Array<DropdownOption<TValue>>;
  label: string;
  onChange?: (value: TValue) => void;
  className?: string;
  buttonClassName?: string;
  disabled?: boolean;
}

export function Dropdown<TValue extends string>({
  value,
  options,
  label,
  onChange,
  className = "",
  buttonClassName = "",
  disabled = false,
}: DropdownProps<TValue>) {
  const [isOpen, setIsOpen] = useState(false);
  const rootRef = useRef<HTMLDivElement | null>(null);
  const menuId = useId();
  const isDisabled = disabled || !onChange;
  const selectedOption = options.find((option) => option.value === value);

  useEffect(() => {
    if (isDisabled) {
      setIsOpen(false);
    }
  }, [isDisabled]);

  useDismissableLayer(isOpen, rootRef, () => setIsOpen(false));

  return (
    <div ref={rootRef} className={`cad-dropdown ${className}`}>
      <button
        type="button"
        className={`cad-dropdown-trigger ${buttonClassName}`}
        disabled={isDisabled}
        aria-label={label}
        aria-haspopup="listbox"
        aria-expanded={isOpen}
        aria-controls={menuId}
        onClick={() => setIsOpen((current) => !current)}
        onKeyDown={(event) => {
          if (isDisabled) {
            return;
          }
          if (event.key === "ArrowDown" || event.key === "Enter") {
            event.preventDefault();
            setIsOpen(true);
          }
        }}
      >
        <span className="cad-dropdown-value">
          {selectedOption?.label ?? value}
        </span>
        <span className="cad-dropdown-chevron" aria-hidden="true" />
      </button>
      {isOpen ? (
        <div
          id={menuId}
          role="listbox"
          aria-label={label}
          className="cad-dropdown-menu cad-scrollbar"
        >
          {options.map((option) => {
            const isSelected = option.value === value;
            return (
              <button
                key={option.value}
                type="button"
                role="option"
                aria-selected={isSelected}
                disabled={option.disabled}
                className={
                  isSelected
                    ? "cad-dropdown-option cad-dropdown-option-selected"
                    : "cad-dropdown-option"
                }
                onClick={() => {
                  if (option.disabled) {
                    return;
                  }
                  onChange?.(option.value);
                  setIsOpen(false);
                }}
              >
                <span>{option.label}</span>
              </button>
            );
          })}
        </div>
      ) : null}
    </div>
  );
}
