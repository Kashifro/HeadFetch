#pragma once

#include "../core/Types.h"
#include <GLES2/gl2.h>
#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_map>

namespace hf::Head {

inline constexpr std::size_t MAX_CACHE_SIZE = 200;

struct TextureEntry {
	GLuint glTex = 0;
	std::chrono::steady_clock::time_point lastUsed;
};

inline std::unordered_map<std::string, TextureEntry> Cache;

inline void processUploads(){
	std::unordered_map<std::string, HeadPixels> pending;
	{
		std::lock_guard<std::mutex> lock(State::PendingHeadsMutex);
		if(State::PendingHeads.empty()){ return; }
		pending.swap(State::PendingHeads);
	}
	GLint prevTex = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
	auto now = std::chrono::steady_clock::now();
	for(auto& [uuid, pixels] : pending){
		GLuint tex;
		auto it = Cache.find(uuid);
		if(it != Cache.end() && it->second.glTex){
			tex = it->second.glTex;
		}else{
			glGenTextures(1, &tex);
			Cache[uuid] = { tex, now };
		}
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, HEAD_TEX_SIZE, HEAD_TEX_SIZE, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		Cache[uuid].lastUsed = now;
	}
	glBindTexture(GL_TEXTURE_2D, (GLuint)prevTex);
}

inline GLuint find(const std::string& uuid){
	auto it = Cache.find(uuid);
	if(it == Cache.end() || !it->second.glTex){ return 0; }
	it->second.lastUsed = std::chrono::steady_clock::now();
	return it->second.glTex;
}

inline void evictStale(std::size_t maxSize = MAX_CACHE_SIZE){
	if(Cache.size() <= maxSize){ return; }
	std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> ages;
	for(const auto& [uuid, e] : Cache){
		ages.emplace_back(uuid, e.lastUsed);
	}
	std::sort(ages.begin(), ages.end(), [](const auto& a, const auto& b){
		return a.second < b.second;
	});
	std::size_t toRemove = Cache.size() - maxSize;
	for(std::size_t i = 0; i < toRemove && i < ages.size(); ++i){
		auto it = Cache.find(ages[i].first);
		if(it != Cache.end()){
			if(it->second.glTex){ glDeleteTextures(1, &it->second.glTex); }
			Cache.erase(it);
		}
	}
}

} // namespace hf::Head
