#pragma once

namespace pl::mod {
class NativeMod;
}

namespace headfetch::Hooks {

// Resolves every target address (see signatures/Signatures.h) and installs
// the corresponding hook through the official pl::hook::hook() API, and
// registers the touch/key callbacks through PreloaderInput. Called from
// HeadFetchMod::enable().
//
// Returns false if any *required* hook failed to install. EGL hooks and the
// PlayerListPacket hook are treated as required (without them the mod cannot
// function); a missing HudCursorRenderer hook only disables render-gating
// (the panel would stay visible slightly longer after leaving the HUD) so it
// is logged as a warning rather than a hard failure.
bool installAll(pl::mod::NativeMod& self);

// Best-effort removal, called from HeadFetchMod::disable(). The underlying
// pl::hook API does not guarantee unhook success once other mods have
// layered hooks on the same address; failures are logged, not fatal.
void removeAll(pl::mod::NativeMod& self);

} // namespace headfetch::Hooks
