# Conversion Report

## Source project classification

- **Project:** [Kashifro/HeadFetch](https://github.com/Kashifro/HeadFetch),
  forked into this repo (`RealBatu20/HeadFetch-LLroidPort`) before this port.
- **Classification:** native C++ shared-library mod for
  [mcpelauncher](https://github.com/minecraft-linux/mcpelauncher-manifest)
  (Minecraft Bedrock on Linux desktop). Not a Fabric/Forge/Bukkit-style
  loader mod -- there is no Java layer at all, and no Gradle build.
- **Target platform (before):** Linux desktop, `x86_64` / `arm64-v8a`,
  loaded by mcpelauncher's native mod loader (`libmcpelauncher_mod.so`).
- **Target platform (after):** Android, `arm64-v8a` (primary),
  `armeabi-v7a` (secondary), loaded by LeviLauncher / LeviLaunchroid's
  `preload-native` mod type.
- **Compliance status:** quality-of-life UI overlay, no gameplay
  modification, no unfair advantage, no anti-cheat interaction. Compliant
  with the scope this assistant supports.
- **Conversion feasibility:** high for the gameplay/rendering logic (it does
  not depend on mcpelauncher at all), medium-risk for the hook installation
  layer, which is genuinely different between the two hosts and is the part
  most in need of on-device verification (see `HOOK_REPORT.md`).

## Why the QYCottage template was used

The [QYCottage LeviLauncher Android Mod Template](https://github.com/QYCottage/levilauncher-android-mod-template)
was used as the scaffold for:

- `CMakeLists.txt` structure: cache variables (`MOD_ID`, `MOD_NAME`, ...),
  the `preloader-android` `FetchContent` block with `LEVI_PRELOADER_ROOT`
  override, the host-side `levi_config_generator` / `levi_generate_config`
  targets, and the `levi_package` packaging target.
- `manifest.json.in` verbatim.
- `scripts/package.ps1` verbatim (it is generic; the mod-specific values
  come from the CMake cache variables, not the script).
- The `src/config_generator.cpp` / `src/mod/Config.cpp` split pattern for
  typed config JSON + schema generation.

### Template files kept as-is
- `manifest.json.in`
- `scripts/package.ps1`

### Template files adapted
- `CMakeLists.txt` -- added `libhat` and `imgui` `FetchContent` blocks and
  linked `GLESv2`/`EGL`/`android`/`log`, added `src/hooks/HookManager.cpp`
  to the mod library's sources; mod identity cache variables set to
  HeadFetch's values instead of the template's placeholders.
- `src/config_generator.cpp` / `src/mod/Config.cpp` -- same shape, renamed
  namespace to `headfetch` and replaced `ModConfig`'s fields.
- `src/main.cpp` -- same `PL_REGISTER_MOD` registration line, pointed at
  `headfetch::HeadFetchMod`.

### Template files expanded beyond the base template
Because HeadFetch does low-level engine hooking and rendering (which the
base template does not attempt), the project uses the "expanded layout"
described for hook/rendering-heavy mods rather than the minimal template
layout:
- `src/hooks/` -- hook installation/removal, isolated from gameplay logic.
- `src/signatures/` -- address resolution, isolated from hook installation.
- `src/game/`, `src/core/`, `src/skin/`, `src/ui/` -- carried over from the
  original mcpelauncher build (see mapping table below).

## File-by-file mapping

| Original (mcpelauncher) | This port | Status |
|---|---|---|
| `src/main.cpp` (`mod_preinit`/`mod_init`) | `src/main.cpp` (`PL_REGISTER_MOD`) | Rewritten: LeviLauncher lifecycle instead of mcpelauncher's two exported C functions. |
| `src/ui/TabList.h` (`PlayerBoard` singleton: hooking + business logic combined) | `src/mod/HeadFetchMod.h/.cpp` (business logic + lifecycle) + `src/hooks/HookManager.h/.cpp` (hook installation) | Rewritten and split. Hook installation now goes through `pl::hook::hook()` instead of manual `mprotect` + vtable overwrite. |
| `src/core/Scanner.h` | `src/signatures/RttiScanner.h` | Kept the same RTTI-walk technique (portable: relies only on Itanium C++ ABI RTTI layout and `dl_iterate_phdr`, both present on Android/Bionic), renamed and reorganized as a pure address *resolver* (no longer also does the patching). |
| `src/core/Types.h` | `src/core/Types.h` | Kept, with `TabHeld`/`PanelOpen`/`ToggleRect` state added for the new touch-based toggle. |
| `src/game/Offsets.h` | `src/game/Offsets.h` | Kept unchanged (see `SIGNATURE_REPORT.md` for verification status). |
| `src/skin/SkinCropper.h` | `src/skin/SkinCropper.h` | Kept unchanged -- pure struct/byte math over engine memory, no dependency on the host loader. |
| `src/skin/TextureCache.h` | `src/skin/TextureCache.h` | Kept, `evictStale()` now takes a runtime cap sourced from the typed config instead of a compile-time constant. |
| `src/ui/ImGuiLayer.h` | `src/ui/ImGuiLayer.h` | Kept, added `drawToggleButton()` for the new tap-to-toggle UX and a runtime `PanelScale` knob. |
| `src/ui/font_data.h` | `src/ui/font_data.h` | Unchanged. |
| (none) | `src/mod/Config.h/.cpp` | New: typed config (`enabled`, `panelScale`, `maxCachedHeads`). |
| (none) | `src/config_generator.cpp` | New: template-pattern config/schema JSON generator, run on the host during packaging. |

## Bedrock system mapping

| Java/mcpelauncher-side concept | Bedrock-native equivalent used |
|---|---|
| mcpelauncher preinit EGL hook | Direct inline hook of `libEGL.so`'s exported `eglSwapBuffers`/`eglMakeCurrent`, resolved via `pl::signature::resolveSignature(name, "libEGL.so")`. |
| mcpelauncher gamewindow keyboard callback | `PreloaderInput`'s `RegisterKeyEventCallback`/`RegisterTouchCallback` (see `HOOK_REPORT.md`). |
| Desktop "hold Tab" gesture | Tap-to-toggle on-screen button (primary, touch-first) + hold-Tab on an attached hardware keyboard (secondary, parity with the original). |
| Manual vtable-slot patch | `pl::hook::hook()` inline hook installed at the address stored in the resolved vtable slot. |

## Feature parity

Full parity is achieved for the core feature (tablist with live skin heads).
The only intentional behavior change is the toggle gesture, which is a
platform-appropriate adaptation (a physical Tab key rarely exists on
Android) rather than a limitation.

## What still needs runtime verification

See `SIGNATURE_REPORT.md`, `HOOK_REPORT.md`, and `TESTING_GUIDE.md`. In
short: this port has not been compiled or run against a real device or a
real `libminecraftpe.so` in this environment (no Android NDK/device is
available here). Everything below FACT-level is INFERENCE (carried over
from a working desktop build of the same engine) or HYPOTHESIS
(new for this port, e.g. the exact `pl::hook`/`PreloaderInput` C++ symbol
names) until built and tested on-device.
