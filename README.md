# HeadFetch (LeviLauncher / LeviLaunchroid port)

A tablist overlay for Minecraft Bedrock on Android, showing every online
player's name next to their live skin head, ported from the
[mcpelauncher build of HeadFetch](https://github.com/Kashifro/HeadFetch) to
run as a native [LeviLauncher](https://github.com/LiteLDev/LeviLaunchroid)
mod.

- **Target Minecraft Bedrock version:** matches whatever is set in
  `MOD_MINECRAFT_VERSIONS` (default `["1.21.*"]`) -- adjust to the version
  you tested against, see `SIGNATURE_REPORT.md`.
- **Target LeviLauncher / LeviLaunchroid version:** whatever
  `preloader-android`'s `main` branch currently targets, or the version you
  pin via `-DLEVI_PRELOADER_ROOT`.
- **Target ABI:** `arm64-v8a` (primary), `armeabi-v7a` (secondary, untested).

## What changed from the mcpelauncher build

mcpelauncher is a desktop Linux launcher; LeviLauncher runs the same
Bedrock engine directly on an Android device. The gameplay logic (decoding
`PlayerListPacket`, cropping the 8x8 head region out of a player's skin,
drawing the ImGui overlay) is unchanged, but the *plumbing* around it had to
be replaced:

| Concern | mcpelauncher build | This port |
|---|---|---|
| Mod entry point | `extern "C" mod_preinit/mod_init` | `PL_REGISTER_MOD` lifecycle class (`load/enable/disable/unload`) |
| Hooking | Manual `mprotect` + vtable slot overwrite, `mcpelauncher_preinithook` for EGL | `pl::hook::hook()` inline hooks on RTTI-resolved / signature-resolved addresses |
| EGL/GL context detection | mcpelauncher preinit hook API | Hooking the real `eglSwapBuffers`/`eglMakeCurrent` exports in `libEGL.so` |
| "Show tablist" input | Hold physical Tab key (desktop) | Tap a persistent on-screen toggle button (touch), or hold Tab on an attached hardware keyboard |
| Config | None (compile-time only) | Typed `pl::config::ConfigFile<ModConfig>` (`enabled`, `panelScale`, `maxCachedHeads`) |
| Packaging | Raw `.so` install into a `mods/` folder | `.levipack` produced by the `levi_package` CMake target |

See `CONVERSION_REPORT.md` for the full file-by-file mapping and
`SIGNATURE_REPORT.md` / `HOOK_REPORT.md` for what is and isn't verified
against a real device yet.

## Requirements

- Android SDK + Android NDK (r28 / 28.2.13676358 recommended) with CMake and
  Ninja components installed.
- PowerShell 7+ (or Windows PowerShell) for `scripts/package.ps1`, or a
  POSIX shell for `scripts/build.sh`.
- Network access to fetch `preloader-android`, `nlohmann/json`, `Boost.PFR`,
  `magic_enum`, `libhat`, and `imgui` via CMake `FetchContent` -- or set
  `-DLEVI_PRELOADER_ROOT=/path/to/local/checkout` to use a local
  `preloader-android` checkout instead (required for offline/reproducible
  builds).

## Building

```powershell
$env:ANDROID_HOME = "C:/Users/<you>/AppData/Local/Android/Sdk"
./scripts/package.ps1 -Abi arm64-v8a
```

or on Linux/macOS:

```bash
export ANDROID_HOME=~/Android/Sdk
./scripts/build.sh arm64-v8a
```

Both scripts:
1. Configure and run the `levi_generate_config` host target to produce
   `config.json` / `config.schema.json` from the typed `ModConfig` struct.
2. Configure and build the Android shared library with the NDK toolchain.
3. Run the `levi_package` CMake target, which strips the `.so`, renders
   `manifest.json` from `manifest.json.in`, and zips everything into
   `build-<abi>/headfetch-<version>-<abi>.levipack`.

To build both `arm64-v8a` and `armeabi-v7a`: pass `-Abi all` /
`./scripts/build.sh all`.

Manual equivalent (see also `scripts/package.ps1`):

```bash
cmake -S . -B build-arm64-v8a \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_HOME/ndk/28.2.13676358/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_STL=c++_shared \
  -DLEVI_PACKAGE_CONFIG_DIR=<path-to-generated-config>
cmake --build build-arm64-v8a --target levi_package
```

## Installing

Import the produced `.levipack` in LeviLauncher's mod manager. On first
launch it writes `config/config.json` (schema in `config/config.schema.json`)
under the mod's data directory, editable from LeviLauncher's config UI.

## Configuration

| Field | Default | Meaning |
|---|---|---|
| `enabled` | `true` | Master on/off switch. |
| `panelScale` | `1.0` | Extra multiplier on top of the automatic screen-height scale (0.5-2.0). |
| `maxCachedHeads` | `200` | Max resident head textures before least-recently-used eviction. |

## Safe feature scope

This mod only reads data the client already receives (the player list
packet and skin bitmaps already sent to the client by the server) and draws
an overlay; it sends nothing extra, alters no gameplay state, and grants no
information a player couldn't already get by opening the vanilla pause-menu
player list. It is a quality-of-life / UI mod, not a gameplay-affecting one.

## EULA / policy note

This mod does not read hidden server state, does not automate combat or
movement, and does not bypass any anti-cheat or access control. It is
intended for use on servers that permit client-side UI mods; check server
rules before use, same as with the original mcpelauncher build.

## Troubleshooting

See `TROUBLESHOOTING.md`. For manual testing steps, see `TESTING_GUIDE.md`.
