#pragma once

#include <string>

#include "pl/cpp/Config.hpp"

namespace pl::mod {
class NativeMod;
}

namespace headfetch {

struct ModConfig {
	int version = 1;
	bool enabled = true;
	// Extra multiplier applied on top of the automatic screen-height scale
	// used when laying out the tablist panel. Lets a user compensate for
	// very small or very large phone/tablet displays.
	double panelScale = 1.0;
	// Maximum resident head textures kept in the GPU texture cache before
	// the least-recently-used entries are evicted.
	int maxCachedHeads = 200;
};

nlohmann::json makeDefaultConfigJson();
nlohmann::json makeConfigSchemaJson();

} // namespace headfetch

template <> struct pl::config::Schema<headfetch::ModConfig> {
	static constexpr std::string_view title = "HeadFetch Config";
	static constexpr std::string_view description = "Player tablist with skin heads.";

	static constexpr FieldSchema field(std::string_view name) {
		if (name == "version")
			return {.title = "Version", .readOnly = true};
		if (name == "enabled")
			return {.title = "Enabled", .description = "Turns the tablist overlay on or off."};
		if (name == "panelScale")
			return {.title = "Panel Scale", .description = "Extra scale multiplier for the tablist panel.",
				.minimum = 0.5, .maximum = 2.0};
		if (name == "maxCachedHeads")
			return {.title = "Max Cached Heads",
				.description = "Maximum number of player head textures kept cached at once.",
				.minimum = 20, .maximum = 1000};
		return {};
	}
};
