#pragma once
#include "../game/Offsets.h"
#include "RttiScanner.h"
#include <cstdint>
#include <pl/cpp/Signature.hpp>

// Central place where every address this mod hooks is resolved. Keeping
// resolution separate from hook installation (hooks/HookManager.cpp) means
// each address can be re-verified or re-derived independently, and a failed
// resolution for one target never prevents the others from being attempted.
namespace hf::Signatures {

// HudCursorRenderer::render and the PlayerListPacket read handler are not
// exported symbols, so we cannot resolve them with
// pl::signature::resolveSignature() by name, and we deliberately avoid a
// hardcoded byte-pattern signature because none has been captured against a
// real Android libminecraftpe.so build. Instead we resolve the actual
// function address through the RTTI vtable-slot technique in RttiScanner.h,
// which only depends on the class name string the compiler embeds in RTTI
// data and is robust across Minecraft versions.
inline std::uintptr_t resolveHudCursorRender(){
	auto ranges = Rtti::findModuleRanges("libminecraftpe.so");
	return Rtti::resolveVirtualSlot(ranges, "17HudCursorRenderer", Game::HudCursor_RenderSlot);
}

inline std::uintptr_t resolvePlayerListPacketRead(){
	auto ranges = Rtti::findModuleRanges("libminecraftpe.so");
	return Rtti::resolveVirtualSlot(ranges, "16PlayerListPacket", Game::PlayerListPacket_ReadSlot);
}

// eglSwapBuffers/eglMakeCurrent are ordinary exported symbols in libEGL.so,
// so the official name-based signature resolver handles them directly --
// this replaces the mcpelauncher-specific mcpelauncher_preinithook() API,
// which does not exist inside a real LeviLauncher-hosted Bedrock process.
inline std::uintptr_t resolveEglSwapBuffers(){
	return pl::signature::resolveSignature("eglSwapBuffers", "libEGL.so");
}

inline std::uintptr_t resolveEglMakeCurrent(){
	return pl::signature::resolveSignature("eglMakeCurrent", "libEGL.so");
}

} // namespace hf::Signatures
