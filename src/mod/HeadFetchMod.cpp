#include "mod/HeadFetchMod.h"

#include "../game/Offsets.h"
#include "../skin/SkinCropper.h"
#include "../skin/TextureCache.h"
#include "../ui/ImGuiLayer.h"
#include "hooks/HookManager.h"

#include <filesystem>
#include <pl/cpp/Config.hpp>
#include <pl/cpp/Mod.hpp>

namespace headfetch {

HeadFetchMod& HeadFetchMod::getInstance() {
	static HeadFetchMod instance;
	return instance;
}

pl::mod::NativeMod& HeadFetchMod::getSelf() const {
	return *pl::mod::NativeMod::current();
}

bool HeadFetchMod::load() {
	auto& self = getSelf();
	self.getLogger().debug("Loading...");

	std::error_code ec;
	std::filesystem::create_directories(self.getConfigDir(), ec);
	if (ec) {
		self.getLogger().error("Failed to create config directory {}: {}", self.getConfigDir().string(), ec.message());
		return false;
	}

	pl::config::ConfigFile<ModConfig> configFile;
	if (!configFile.load()) {
		self.getLogger().warn("Failed to load typed config, using defaults");
	} else {
		m_config = configFile.value();
	}

	UI::PanelScale = static_cast<float>(m_config.panelScale);
	self.getLogger().info("Loaded {} from {}", self.getName(), self.getModDir().string());
	return true;
}

bool HeadFetchMod::enable() {
	auto& self = getSelf();
	self.getLogger().debug("Enabling...");
	if (!m_config.enabled) {
		self.getLogger().info("HeadFetch is disabled by config");
		return true;
	}
	if (!Hooks::installAll(self)) {
		self.getLogger().error("One or more required hooks failed to install; HeadFetch may not function");
		return false;
	}
	return true;
}

bool HeadFetchMod::disable() {
	auto& self = getSelf();
	self.getLogger().debug("Disabling...");
	Hooks::removeAll(self);
	return true;
}

bool HeadFetchMod::unload() {
	getSelf().getLogger().debug("Unloading...");
	std::lock_guard<std::mutex> lock(m_cacheMutex);
	m_cache.clear();
	{
		std::lock_guard<std::mutex> lock2(State::PlayersMutex);
		State::Players.clear();
	}
	{
		std::lock_guard<std::mutex> lock3(State::PendingHeadsMutex);
		State::PendingHeads.clear();
	}
	return true;
}

void HeadFetchMod::onHudRender() {
	State::LastHudTime = State::monotonicNow();
}

void HeadFetchMod::onPacketRead(void* packet) {
	if (!packet) { return; }
	auto addr = reinterpret_cast<std::uintptr_t>(packet);
	auto begin  = *reinterpret_cast<std::uintptr_t*>(addr + Game::PLP_EntriesBegin);
	auto end    = *reinterpret_cast<std::uintptr_t*>(addr + Game::PLP_EntriesEnd);
	auto action = *reinterpret_cast<std::uint8_t*>(addr + Game::PLP_Action);
	if (!begin || !end || end < begin) { return; }
	auto bytes = end - begin;
	if (bytes % Game::PLP_EntrySize != 0) { return; }
	auto count = bytes / Game::PLP_EntrySize;
	if (count == 0 || count > 300) { return; }

	std::lock_guard<std::mutex> lock(m_cacheMutex);
	if (action == 0 && count > 1) { m_cache.clear(); }
	for (std::size_t i = 0; i < count; ++i) {
		auto entry = begin + i * Game::PLP_EntrySize;
		auto uuid = *reinterpret_cast<const UUIDRaw*>(entry + Game::PLP_EntryUuid);
		if (action == 0) {
			auto* nameStr = reinterpret_cast<const std::string*>(entry + Game::PLP_EntryName);
			if (nameStr && !nameStr->empty() && nameStr->size() <= 64) {
				auto uuidStr = uuidToString(uuid);
				HeadPixels head{};
				if (Head::extractFromEntry(entry, head)) {
					std::lock_guard<std::mutex> hl(State::PendingHeadsMutex);
					State::PendingHeads[uuidStr] = std::move(head);
				}
				upsert(uuid, uuidStr, *nameStr);
			}
		} else if (action == 1) {
			removeByUuid(uuid);
		}
	}
	publishPlayers();
}

void HeadFetchMod::onSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
	if (!m_contextReady) { return; }
	EGLint w = 0, h = 0;
	eglQuerySurface(dpy, surface, EGL_WIDTH, &w);
	eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);
	Head::processUploads();
	if (++m_evictCounter % 300 == 0) {
		Head::evictStale(static_cast<std::size_t>(std::max(1, m_config.maxCachedHeads)));
	}
	if (w <= 0 || h <= 0) { return; }
	if (State::monotonicNow() - State::LastHudTime.load(std::memory_order_relaxed) >= 2.0) { return; }

	UI::beginFrame((float)w, (float)h);
	UI::drawToggleButton((float)w, (float)h, std::clamp((float)h / 1080.0f, 0.5f, 1.5f));
	if (State::panelVisible()) {
		std::vector<PlayerInfo> players;
		{
			std::lock_guard<std::mutex> lock(State::PlayersMutex);
			players = State::Players;
		}
		if (!players.empty()) {
			UI::drawList(players, (float)w, (float)h);
		}
	}
	UI::endFrame();
}

void HeadFetchMod::onMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLContext ctx) {
	if (!m_contextReady && draw != EGL_NO_SURFACE && ctx != EGL_NO_CONTEXT) {
		m_contextReady = true;
		UI::init();
	}
}

bool HeadFetchMod::onTouch(int action, int pointerId, float x, float y) {
	// Android MotionEvent.ACTION_DOWN == 0. Only react to the initial touch
	// of a gesture; ignore move/up so a drag that starts on the button but
	// ends elsewhere (or vice versa) does not cause a spurious toggle.
	constexpr int ACTION_DOWN = 0;
	if (action != ACTION_DOWN) { return false; }
	ToggleButtonRect rect;
	{
		std::lock_guard<std::mutex> lock(State::ToggleRectMutex);
		rect = State::ToggleRect;
	}
	if (!rect.valid) { return false; }
	if (x < rect.x0 || x > rect.x1 || y < rect.y0 || y > rect.y1) { return false; }
	State::PanelOpen.store(!State::PanelOpen.load(std::memory_order_relaxed), std::memory_order_relaxed);
	return true;
}

bool HeadFetchMod::onKeyEvent(int keyCode, unsigned int /*unicodeChar*/, bool isKeyDown) {
	// Android KEYCODE_TAB == 61. Only meaningful with an attached hardware
	// keyboard; touch users rely on the on-screen toggle button instead.
	constexpr int KEYCODE_TAB = 61;
	if (keyCode == KEYCODE_TAB) {
		State::TabHeld.store(isKeyDown, std::memory_order_relaxed);
	}
	return false;
}

std::string HeadFetchMod::uuidToString(const UUIDRaw& uuid) {
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%016llx%016llx",
		(unsigned long long)uuid.a, (unsigned long long)uuid.b);
	return buf;
}

void HeadFetchMod::upsert(const UUIDRaw& uuid, const std::string& uuidStr, const std::string& name) {
	for (auto& p : m_cache) {
		if (p.uuid == uuid) { p.name = name; return; }
	}
	m_cache.push_back({uuid, uuidStr, name});
}

void HeadFetchMod::removeByUuid(const UUIDRaw& uuid) {
	m_cache.erase(std::remove_if(m_cache.begin(), m_cache.end(),
		[&](const CachedPlayer& p){ return p.uuid == uuid; }),
		m_cache.end());
}

void HeadFetchMod::publishPlayers() {
	std::vector<PlayerInfo> players;
	for (const auto& p : m_cache) {
		if (!p.name.empty() && p.name.size() <= 64) {
			players.push_back({p.name, p.uuidString});
		}
	}
	std::lock_guard<std::mutex> lock(State::PlayersMutex);
	State::Players = std::move(players);
}

} // namespace headfetch
