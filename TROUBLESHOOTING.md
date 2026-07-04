# Troubleshooting

## Build fails: `LEVI_PRELOADER_ROOT must point to a preloader-android checkout`
CMake could not fetch or find `preloader-android`. Either allow network
access for the `FetchContent_Declare(preloader_android ...)` step, or pass
`-DLEVI_PRELOADER_ROOT=/path/to/local/preloader-android` (or set the
`LEVI_PRELOADER_ROOT` environment variable) pointing at a checkout that
contains `src/pl/cpp/Mod.hpp`.

## Build fails: cannot find `pl::hook::unhook` / `GetPreloaderInput`
These are flagged as HYPOTHESIS in `HOOK_REPORT.md` -- their exact
namespace/signature was not directly confirmed against the
`preloader-android` headers when this port was written. Open
`pl/cpp/Hook.hpp` and `pl/cpp/PreloaderInput.hpp` inside your
`preloader-android` checkout and adjust `src/hooks/HookManager.cpp`
accordingly (fall back to the documented C API, `pl_unhook`/
`GetPreloaderInput()` from `pl/c/Hook.h` / `pl/c/PreloaderInput.h`, if the
C++ wrapper differs).

## Mod loads but the tablist never appears
1. Check the mod log for `HookManager::installAll` warnings. If
   `HudCursorRenderer::render` or the `PlayerListPacket` handler failed to
   resolve, the Minecraft version's RTTI class names or vtable slot indices
   may have shifted -- see `SIGNATURE_REPORT.md` for how to re-derive them.
2. If both engine hooks report success but nothing renders, check whether
   `eglSwapBuffers`/`eglMakeCurrent` hooks installed -- without those, the
   ImGui context never initializes and no frame is ever drawn.
3. Confirm you actually tapped the toggle button (top-right of the HUD) or
   are holding Tab on an attached keyboard -- the panel is closed by
   default.

## Toggle button doesn't respond to touch
The touch hit-test in `HeadFetchMod::onTouch` compares raw
`MotionEvent` coordinates against `State::ToggleRect`, which is published
by `UI::drawToggleButton` in the same coordinate space as `ImGui::GetIO().DisplaySize`
(i.e. `eglQuerySurface`'s reported width/height). If your device reports
touch coordinates in a different space (e.g. pre-DPI-scaling vs.
post-DPI-scaling), the rect comparison will silently miss. Log both
`x, y` from `onTouch` and the current `ToggleRect` to compare, and adjust
the coordinate conversion if they don't line up -- this is called out as
unverified in `HOOK_REPORT.md`.

## Panel appears but heads are blank/black squares
`SkinCropper.h::findSkinImage` failed its sanity checks (format/width/
height/depth or byte-count bounds) for that player's skin. This usually
means `Game::PLP_EntrySkinRef` or the scanned image header layout drifted
for the current Minecraft version. Re-derive both from a decompiled
`PlayerListPacket` entry and `Image` class for your target version.

## Overlay disappears/crashes after backgrounding the app
This is the least-verified code path in the port (see HOOK_REPORT.md,
"Android validation status"). `HeadFetchMod::onMakeCurrent` only
re-initializes ImGui the *first* time a valid context appears
(`m_contextReady` latches true permanently); if the OS destroys and
recreates the EGL context on resume, ImGui's OpenGL objects may need to be
recreated too. If you observe this, change `m_contextReady` handling to
call `ImGui_ImplOpenGL3_Shutdown()`/re-`Init()` (or a full context
teardown/rebuild) whenever `eglMakeCurrent` reports a *new* `EGLContext`
value rather than only reacting the first time.

## Config changes don't take effect
Typed config is only re-read in `load()`, which runs once when the mod
loads -- not on every `enable()`. Restart the game (or fully unload/reload
the mod if LeviLauncher supports that) after editing `config.json`.

## `.levipack` is missing `config/config.json`
`levi_package` copies whatever `-DLEVI_PACKAGE_CONFIG_DIR` points at; make
sure `levi_generate_config` ran first (both `scripts/package.ps1` and
`scripts/build.sh` do this automatically -- if building manually, run
`cmake --build <host-build-dir> --target levi_generate_config` before the
Android build and pass its `generated-config` output directory in).
