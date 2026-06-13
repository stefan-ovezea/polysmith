import { create } from "zustand";

export type ToastLevel = "info" | "warn" | "error";

export interface ToastNotification {
  id: string;
  level: ToastLevel;
  message: string;
  createdAt: number;
}

interface ToastStoreState {
  toasts: ToastNotification[];
  pushToast: (level: ToastLevel, message: string) => void;
  dismissToast: (id: string) => void;
  clearToasts: () => void;
}

const TOAST_LIMIT = 5;

function toastKey(level: ToastLevel, message: string) {
  return `${level}:${message.trim()}`;
}

export const useToastStore = create<ToastStoreState>((set, get) => ({
  toasts: [],
  pushToast: (level, message) => {
    const normalizedMessage = message.trim();
    if (!normalizedMessage) {
      return;
    }

    const key = toastKey(level, normalizedMessage);
    if (
      get().toasts.some(
        (toast) => toastKey(toast.level, toast.message) === key,
      )
    ) {
      return;
    }

    const toast: ToastNotification = {
      id:
        globalThis.crypto?.randomUUID?.() ??
        `${Date.now()}-${Math.random().toString(36).slice(2)}`,
      level,
      message: normalizedMessage,
      createdAt: Date.now(),
    };
    set((state) => ({
      toasts: [...state.toasts, toast].slice(-TOAST_LIMIT),
    }));
  },
  dismissToast: (id) =>
    set((state) => ({
      toasts: state.toasts.filter((toast) => toast.id !== id),
    })),
  clearToasts: () => set({ toasts: [] }),
}));
