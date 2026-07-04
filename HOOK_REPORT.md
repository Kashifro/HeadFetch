# Hook Report

## API used

All hooks go through the official LeviLauncher hook API, confirmed against
`https://levilaunchroid.levimc.org/api/hook` (fetched in this session):

```c
PLAPI int pl_hook(PLFuncPtr target, PLFuncPtr detour, PLFuncPtr *originalFunc, PLHookPriority priority);
PLAPI bool pl_unhook(PLFuncPtr target, PLFuncPtr detour);
```

with a C++ wrapper `pl::hook::hook(target, detour, &original, pl::hook::PriorityNormal)`.
This project calls the C++ wrapper, matching the exact shape of the example
in the docs (target/detour/original passed as plain `void*`/`void**` via
`reinterpret_cast`, not a named `PLFuncPtr` type, since the docs' own C++
example did not spell that type out).

**FACT:** `pl::hook::hook`'s signature, priority enum values, and the
trampoline pattern (call through `*originalFunc`, never the raw target
address) as shown above.

**HYPOTHESIS:** `pl::hook::unhook`'s exact C++ signature. Only the C
`pl_unhook(PLFuncPtr, PLFuncPtr)` form was shown in the fetched docs; this
project assumes a mirrored `pl::hook::unhook(void*, void*)` in
`pl/cpp/Hook.hpp`. **If this does not compile**, replace the calls in
`src/hooks/HookManager.cpp::removeAll` with the C API directly:
`pl_unhook(reinterpret_cast<PLFuncPtr>(addr), reinterpret_cast<PLFuncPtr>(&detourFn))`,
including `<pl/c/Hook.h>`.

## Hooks installed

| Target | Resolution | Priority | Purpose |
|---|---|---|---|
| `HudCursorRenderer::render` | RTTI vtable slot (see `SIGNATURE_REPORT.md`) | `PriorityNormal` | Marks "HUD is currently being drawn" so the overlay only shows while the player is actually in a HUD-rendering state (`State::LastHudTime`). |
| `PlayerListPacket` read handler | RTTI vtable slot | `PriorityNormal` | Decodes the player list (name, UUID, skin pointer) as packets arrive. |
| `eglSwapBuffers` | Name resolution in `libEGL.so` | `PriorityNormal` | Frame-end hook: uploads pending head textures, evicts stale cache entries, draws the ImGui overlay. |
| `eglMakeCurrent` | Name resolution in `libEGL.so` | `PriorityNormal` | Detects the first valid EGL context to initialize ImGui exactly once. |

All four use `PriorityNormal` since none of them need to run before or
after another mod's hook on the same target specifically -- there is no
ordering dependency within this mod's own hooks (each targets a distinct
function).

## Input registration

Confirmed against `https://levilaunchroid.levimc.org/api/input` (fetched in
this session):

```c
#include <pl/c/PreloaderInput.h>      // or pl/cpp/PreloaderInput.hpp for C++
GetPreloaderInput()->RegisterTouchCallback(bool(*)(int action, int pointerId, float x, float y));
GetPreloaderInput()->RegisterKeyEventCallback(bool(*)(int keyCode, unsigned int unicodeChar, bool isKeyDown));
```

**FACT:** callback signatures, that `action` is the raw Android
`MotionEvent` action int, and that "there is currently no unregister API --
avoid registering the same callback repeatedly" (quoted from the docs).
Because of that last point, `HookManager::installAll` registers the touch
and key callbacks exactly once per process; `HeadFetchMod::disable()` does
**not** attempt to unregister them, and the detour functions are written to
be safe to keep receiving events after `disable()` (see below).

**HYPOTHESIS:** the exact enclosing namespace of the free function
`GetPreloaderInput()` (the docs summary did not show one explicitly). If
`GetPreloaderInput` fails to resolve at link/compile time, check
`pl/cpp/PreloaderInput.hpp` for the correct namespace and qualify the call
in `src/hooks/HookManager.cpp`.

### Why re-registration is safe across disable/enable
`onTouch`/`onKeyEvent` in `HeadFetchMod` only mutate atomics
(`State::TabHeld`, `State::PanelOpen`) and read a mutex-guarded rect
(`State::ToggleRect`). If the mod is disabled and re-enabled within the same
process (which `enable()`/`disable()` do not currently guard against
happening more than once), the callbacks stay functionally correct -- they
just keep toggling state that only matters while the render hook is also
active. No crash risk, only a latent "toggle state persists across a
disable" quirk, which is cosmetic (the panel simply won't render while
disabled, regardless of the toggle bit).

## Detour reentrancy note

`eglSwapDetour`/`eglMakeCurrentDetour`/`hudRenderDetour`/`packetReadDetour`
all null-check their captured `g_orig*` pointer before calling through it,
so a hook that failed to resolve `originalFunc` (should not happen if
`pl::hook::hook` returned success, but guarded defensively) fails safe
instead of calling through a null function pointer.

## Android validation status

Not yet performed in this environment (no Android device, emulator, or NDK
toolchain available in this session). Required before shipping:
- Confirm `pl::hook::hook`/`unhook` and `GetPreloaderInput` compile and link
  against a real `preloader-android` checkout.
- Confirm both RTTI-resolved targets are found and the hooks install
  successfully (check `getLogger()` output at `enable()` time).
- Confirm the overlay renders correctly across orientation changes and
  `eglMakeCurrent` context recreation (e.g. app backgrounding/foregrounding).
- Confirm the on-screen toggle button's hit-test coordinates line up with
  actual touch coordinates on at least one physical device (coordinate
  space/DPI scaling assumptions are otherwise unverified).
