#pragma once

#include "../core/Types.h"
#include "Config.h"
#include <EGL/egl.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace pl::mod {
class NativeMod;
}

namespace headfetch {

// Replaces the mcpelauncher-era PlayerBoard singleton. Owns the same
// gameplay-facing logic (packet decoding, head texture caching, panel
// visibility state) but no longer owns hook *installation* -- that is
// HookManager's job -- so this class stays a plain event sink that is easy
// to unit-reason about and matches the load/enable/disable/unload lifecycle
// LeviLauncher drives a NativeMod through.
class HeadFetchMod {
public:
	static HeadFetchMod& getInstance();

	[[nodiscard]] pl::mod::NativeMod& getSelf() const;

	bool load();
	bool enable();
	bool disable();
	bool unload();

	// --- Event sink, called from the detours in hooks/HookManager.cpp ---
	void onHudRender();
	void onPacketRead(void* packet);
	void onSwapBuffers(EGLDisplay dpy, EGLSurface surface);
	void onMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLContext ctx);
	// Returns true if the event was consumed (hit the toggle button) and
	// should not be forwarded to the game's own input handling.
	bool onTouch(int action, int pointerId, float x, float y);
	bool onKeyEvent(int keyCode, unsigned int unicodeChar, bool isKeyDown);

	[[nodiscard]] const ModConfig& config() const { return m_config; }

private:
	HeadFetchMod() = default;

	static std::string uuidToString(const UUIDRaw& uuid);
	void upsert(const UUIDRaw& uuid, const std::string& uuidStr, const std::string& name);
	void removeByUuid(const UUIDRaw& uuid);
	void publishPlayers();

	ModConfig m_config;
	bool m_contextReady = false;
	int m_evictCounter = 0;
	std::mutex m_cacheMutex;
	std::vector<CachedPlayer> m_cache;
};

} // namespace headfetch
