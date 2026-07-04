# Testing Guide

No Android device, emulator, or NDK toolchain is available in the
environment this port was written in, so none of the steps below have been
executed. This is a checklist for whoever builds and installs the
`.levipack` on real hardware.

## 1. Build sanity
- [ ] `./scripts/build.sh arm64-v8a` (or `package.ps1`) completes without
      CMake configure errors -- this alone validates that `preloader-android`
      exposes `pl/cpp/Mod.hpp`, `pl/cpp/Signature.hpp`, `pl/cpp/Hook.hpp` (or
      wherever `pl::hook` actually lives -- see `HOOK_REPORT.md`), and
      `pl/cpp/PreloaderInput.hpp` at the paths this project assumes.
- [ ] The build produces `build-arm64-v8a/headfetch-0.1.0-arm64-v8a.levipack`.
- [ ] Unzip the `.levipack` and confirm it contains `manifest.json`,
      `libheadfetch.so`, and `config/config.json` + `config/config.schema.json`.

## 2. Install and load
- [ ] Import the `.levipack` in LeviLauncher's mod manager.
- [ ] Launch Minecraft Bedrock with the mod enabled; confirm no crash on
      startup (this exercises `load()` and `enable()`, including all four
      hook installs and the `PreloaderInput` registration).
- [ ] Check LeviLauncher's mod log for the `[warn]`/`[error]` messages
      `HookManager::installAll` emits on any resolution/hook failure.

## 3. Core feature
- [ ] Join a world/server with at least one other player online.
- [ ] Tap the small toggle button that appears near the top-right of the
      HUD; confirm the tablist panel appears, listing all connected
      players with their skin heads.
- [ ] Confirm player heads match each player's actual worn skin (front-face
      head + hat-layer overlay, not just a flat color).
- [ ] Tap the toggle again; confirm the panel closes but the toggle button
      remains visible.
- [ ] If a hardware keyboard is attached, hold Tab; confirm the panel opens
      while held and closes on release, independent of the toggle button's
      latched state.
- [ ] Have a player leave; confirm they disappear from the list on the next
      packet update.
- [ ] Join/leave repeatedly; confirm no leak or crash from the head texture
      cache (watch GPU memory if possible) and that `maxCachedHeads`
      eviction kicks in after ~300 frames per `HeadFetchMod::onSwapBuffers`.

## 4. Lifecycle robustness
- [ ] Background the app (Home button) and foreground it again; confirm the
      overlay keeps rendering after the `eglMakeCurrent` context is
      recreated (this is the least-verified part of the port -- see
      `HOOK_REPORT.md`'s Android validation status).
- [ ] Rotate the device (if the target device/orientation lock allows it);
      confirm the panel and toggle button reposition correctly for the new
      `sw`/`sh`.
- [ ] Disable the mod from LeviLauncher without closing the game, if
      supported; confirm no crash (`disable()` unhooks all four targets).

## 5. Config
- [ ] Edit `config/config.json` (`panelScale`, `maxCachedHeads`, `enabled`)
      through LeviLauncher's config UI or by hand; confirm the new values
      take effect on the next `load()` (i.e. next mod/game start).
- [ ] Set `enabled: false`; confirm `enable()` logs "disabled by config" and
      no hooks are installed (check the mod log), and no overlay appears.

## 6. Multi-mod compatibility (if feasible)
- [ ] Load alongside another mod that also hooks `HudCursorRenderer` or
      `PlayerListPacket`'s read path, if one is available, to confirm
      `PriorityNormal` ordering does not cause a conflict.
