#pragma once
#include <cstddef>

// [INFERENCE] These layout constants are carried over unchanged from the
// mcpelauncher (Linux-hosted Bedrock) build of HeadFetch. mcpelauncher runs
// the same libminecraftpe.so C++ codebase that ships in the official Android
// APK, so class layouts and vtable slot indices are expected to match for the
// same Minecraft Bedrock version. They are NOT re-verified against a real
// Android libminecraftpe.so in this port -- do that before relying on them:
//   1. Load the target libminecraftpe.so for your exact Bedrock version in
//      IDA Pro (or Ghidra) with the IDA Pro MCP workflow described in
//      SIGNATURE_REPORT.md.
//   2. Re-run the RTTI vtable-slot resolution documented there and confirm
//      the resolved function matches HudCursorRenderer::render / the
//      PlayerListPacket packet-read handler.
//   3. Re-derive PLP_* struct offsets from the decompiled PlayerListPacket
//      entry type if the Minecraft version differs from the one this was
//      last verified against.
namespace hf::Game {
constexpr std::size_t HudCursor_RenderSlot = 17;
constexpr std::size_t PlayerListPacket_ReadSlot = 17;
constexpr std::size_t PLP_EntriesBegin = 0x30;
constexpr std::size_t PLP_EntriesEnd = 0x38;
constexpr std::size_t PLP_Action = 0x48;
constexpr std::size_t PLP_EntrySize = 0x90;
constexpr std::size_t PLP_EntryUuid = 0x08;
constexpr std::size_t PLP_EntryName = 0x18;
constexpr std::size_t PLP_EntrySkinRef = 0x68;
constexpr std::size_t Image_BytesOffset = 0x18;
constexpr std::size_t Image_BytesSizeOffset = 0x28;
constexpr std::size_t MaxSkinImageScan = 0x180;
} // namespace hf::Game
