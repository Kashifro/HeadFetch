# Signature Report

## Classification legend
- **FACT** -- verified directly (source, docs, or decompilation performed in
  this session).
- **INFERENCE** -- supported by evidence (the same engine source shipping on
  both platforms) but not independently re-verified against a real Android
  `libminecraftpe.so` in this session.
- **HYPOTHESIS** -- needs runtime/binary validation before being trusted.

## `libminecraftpe.so` targets

### `HudCursorRenderer::render` (vtable slot 17)
- **INFERENCE.** Value carried over unchanged from the mcpelauncher build of
  HeadFetch (`src/game/Offsets.h: HudCursor_RenderSlot = 17`). mcpelauncher
  runs the same compiled `libminecraftpe.so` that ships in the Android APK
  for a given Bedrock release, so the vtable layout is expected to match for
  the *same* Minecraft version. Not re-verified against a real Android
  binary in this session (no IDA Pro MCP / device access available here).
- **Resolution technique (unchanged):** find the `"17HudCursorRenderer"`
  RTTI type-name string in the module's rodata segment, walk backward to
  the enclosing `type_info` object, walk backward again to the vtable, read
  the pointer at `slot * sizeof(void*)`. Implemented in
  `src/signatures/RttiScanner.h::resolveVirtualSlot`.
- **Before hooking on a new Minecraft version:** open the target
  `libminecraftpe.so` in IDA Pro (via the IDA Pro MCP workflow), locate
  `HudCursorRenderer`'s vtable, and confirm slot 17 is still `render`
  (or whichever draw entrypoint gates HUD-active state). Update
  `Game::HudCursor_RenderSlot` if the slot index moved.

### `PlayerListPacket` read handler (vtable slot 17)
- **INFERENCE**, same reasoning and same caveats as above. RTTI name used:
  `"16PlayerListPacket"`.
- **Struct offsets** (`PLP_EntriesBegin`, `PLP_EntriesEnd`, `PLP_Action`,
  `PLP_EntrySize`, `PLP_EntryUuid`, `PLP_EntryName`, `PLP_EntrySkinRef`) are
  likewise carried over unverified. These are the offsets most likely to
  silently break on a version bump (a struct layout change does not change
  the RTTI name, so `resolveVirtualSlot` would keep "succeeding" while
  reading garbage). `HeadFetchMod::onPacketRead` bounds-checks
  `count <= 300` and `bytes % PLP_EntrySize == 0` specifically to fail safe
  (skip the packet) rather than crash if these drift, but a version bump
  should still trigger a re-check via decompilation.

### Skin image header (`RawImageHeader`, `Image_BytesOffset`, `Image_BytesSizeOffset`)
- **INFERENCE**, unchanged from the original. `SkinCropper.h::findSkinImage`
  additionally sanity-checks `format`/`width`/`height`/`depth` and the
  pixel-buffer byte count before trusting a scanned offset, which limits the
  blast radius of a wrong offset to "no head texture extracted" rather than
  memory corruption.

## `libEGL.so` targets

### `eglSwapBuffers`, `eglMakeCurrent`
- **FACT** that these are standard EGL exports present on Android; **FACT**
  that `pl::signature::resolveSignature(name, moduleName)` supports
  resolving by exported symbol name per the LeviLauncher signature API docs
  (`https://levilaunchroid.levimc.org/api/signature`, fetched in this
  session: "resolveSignature(...) resolves function symbols or byte
  sequences within loaded modules").
- No byte-pattern signature is used for these -- name resolution is strictly
  more reliable for exported ELF symbols and avoids the version-fragility a
  byte pattern would introduce.

## Pattern format reference

Per the fetched signature API docs, byte patterns use `?`/`??` for a whole
wildcard byte, `A?`/`?F` for nibble wildcards (e.g. `"48 8B ?? ?? 89"`). This
project does not currently use any byte-pattern signatures -- everything is
either RTTI-resolved (engine internals with no exported symbol) or
name-resolved (exported EGL symbols) -- but this is documented here in case
a future Minecraft version renames or inlines away the RTTI-scanned classes
and a byte-pattern fallback becomes necessary.
