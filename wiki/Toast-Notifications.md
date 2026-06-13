# Toast Notifications

PolySmith uses a global toast surface for short-lived user-facing notices that
should appear above the main CAD workspace. The implementation lives in:

- `apps/desktop-ui/src/state/toastStore.ts`
- `apps/desktop-ui/src/layout/ToastViewport.tsx`

`ToastViewport` is rendered once at the app root in `App.tsx`. Do not render
additional toast containers in feature panels or viewport components.

## When To Use

Use toasts for transient, actionable feedback that the user should see even if
the logs panel is closed:

- Core command failures and bridge errors
- User-visible warnings that do not require a modal
- Informational completion notices for short actions

Do not use toasts for persistent state, inline validation near an input, or
details that belong in the logs window.

## API

Use the store action:

```ts
import { useToastStore } from "@/state";

useToastStore.getState().pushToast("error", "Something failed");
```

React components may also select the action:

```ts
const pushToast = useToastStore((state) => state.pushToast);
pushToast("info", "Export finished");
```

Supported levels are:

- `info`
- `warn`
- `error`

## Behavior

Toasts appear near the upper-right corner above panels and modals, offset below
the app chrome, auto-dismiss
after 5 seconds, and include a manual `X` close button. Active duplicate toasts
with the same level and message are throttled so bursts of identical core
errors produce only one visible notification.

The CAD core event bridge and core-message store already route core errors to
error toasts. Feature code should not add a second toast for the same core
failure unless it is replacing the message with a clearer user-facing one.
