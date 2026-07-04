#pragma once
// Shared state and value types used across the mod. Unchanged from the
// mcpelauncher build: these are plain data structures with no dependency on
// the host loader, so they carry over to LeviLauncher as-is.
#include <array>
#include <atomic>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace hf {

inline constexpr int HEAD_TEX_SIZE = 64;
using HeadPixels = std::array<std::uint8_t, HEAD_TEX_SIZE * HEAD_TEX_SIZE * 4>;

struct PlayerInfo {
	std::string name;
	std::string uuid;
};

struct UUIDRaw { std::uint64_t a, b; };
inline bool operator==(const UUIDRaw& l, const UUIDRaw& r){
	return l.a == r.a && l.b == r.b;
}

struct CachedPlayer {
	UUIDRaw     uuid;
	std::string uuidString;
	std::string name;
};

// Hit-test rectangle for the on-screen toggle button, published by the UI
// layer each frame and consumed by the Android touch callback registered
// through PreloaderInput. Replaces the desktop "hold Tab" gesture, which has
// no Android equivalent without a physical keyboard.
struct ToggleButtonRect {
	float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	bool valid = false;
};

namespace State {
	inline std::vector<PlayerInfo>  Players;
	inline std::mutex               PlayersMutex;
	inline std::unordered_map<std::string, HeadPixels> PendingHeads;
	inline std::mutex               PendingHeadsMutex;

	// Panel visibility: true while an external-keyboard Tab key is held, or
	// latched by tapping the on-screen toggle button.
	inline std::atomic<bool>        TabHeld{false};
	inline std::atomic<bool>        PanelOpen{false};
	inline std::atomic<double>      LastHudTime{0.0};

	inline std::mutex               ToggleRectMutex;
	inline ToggleButtonRect         ToggleRect;

	inline double monotonicNow(){
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return (double)ts.tv_sec + ts.tv_nsec / 1e9;
	}

	inline bool panelVisible(){
		return TabHeld.load(std::memory_order_relaxed) || PanelOpen.load(std::memory_order_relaxed);
	}
}

} // namespace hf
