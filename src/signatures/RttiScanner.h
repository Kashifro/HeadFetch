#pragma once
// Portable RTTI-based virtual-function resolver.
//
// Bionic/glibc both expose dl_iterate_phdr and Itanium C++ ABI RTTI layout,
// so this technique (find the mangled class-name string emitted by the
// compiler's typeinfo-name object, walk backwards to the typeinfo object,
// walk backwards again to the vtable, then read the target slot) works
// identically on desktop mcpelauncher and on real Android. It is preferred
// over a hardcoded byte-pattern signature because it survives most
// version-to-version code changes as long as RTTI is enabled (Bedrock's
// libminecraftpe.so is not built with -fno-rtti).
//
// This header only *locates* the address stored in a vtable slot -- it does
// not patch anything. Installing the actual hook is done through the
// official pl::hook::hook() API in hooks/HookManager.cpp, which is safer and
// more maintainable than manually mprotect()-ing and overwriting the vtable
// slot in place (the approach the original mcpelauncher build used).
#include <libhat.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include <string>
#include <vector>

namespace hf::Rtti {

struct ModuleRanges {
	void*  segment1Start = nullptr;
	std::size_t segment1Len = 0;
	void*  segment2Start = nullptr;
	std::size_t segment2Len = 0;
	bool   found = false;
};

// Locates the second and third PT_LOAD segments of a loaded module by name
// fragment (e.g. "libminecraftpe.so"). These segments hold, respectively,
// the typeinfo-name strings and the typeinfo/vtable pointer tables on the
// standard Android NDK/clang toolchain layout used to build Bedrock.
inline ModuleRanges findModuleRanges(const char* nameFragment){
	struct Ctx { const char* fragment; ModuleRanges ranges; };
	Ctx ctx{nameFragment, {}};
	dl_iterate_phdr([](struct dl_phdr_info* info, size_t, void* data) -> int {
		auto* c = static_cast<Ctx*>(data);
		if(!info->dlpi_name || !std::strstr(info->dlpi_name, c->fragment)){ return 0; }
		if(info->dlpi_phnum < 3){ return 0; }
		c->ranges.segment1Start = reinterpret_cast<void*>(info->dlpi_addr + info->dlpi_phdr[1].p_vaddr);
		c->ranges.segment1Len   = info->dlpi_phdr[1].p_memsz;
		c->ranges.segment2Start = reinterpret_cast<void*>(info->dlpi_addr + info->dlpi_phdr[2].p_vaddr);
		c->ranges.segment2Len   = info->dlpi_phdr[2].p_memsz;
		c->ranges.found = true;
		return 1;
	}, &ctx);
	return ctx.ranges;
}

inline std::vector<std::uintptr_t> findAllInRange(const void* rangeStart, std::size_t rangeLen, const char* name){
	std::vector<std::uintptr_t> results;
	auto nameLen = std::strlen(name);
	if(nameLen == 0 || rangeLen < nameLen){ return results; }
	const auto* hay = static_cast<const std::uint8_t*>(rangeStart);
	for(std::size_t i = 0; i + nameLen <= rangeLen; ++i){
		if(std::memcmp(hay + i, name, nameLen) == 0){
			results.push_back(reinterpret_cast<std::uintptr_t>(hay + i));
		}
	}
	return results;
}

inline std::uintptr_t findPtrInRange(const void* rangeStart, std::size_t rangeLen, std::uintptr_t value){
	auto span = std::span<const std::byte>(static_cast<const std::byte*>(rangeStart), rangeLen);
	auto result = hat::find_pattern(span, hat::object_to_signature(value));
	auto* found = result.get();
	return found ? reinterpret_cast<std::uintptr_t>(found) : 0;
}

// Resolves the function address stored at `slot` (0-indexed, past the two
// leading offset-to-top/typeinfo vtable header words) in the vtable of the
// class whose mangled name matches `mangledClassNameFragment`.
//
// Returns 0 if the class's typeinfo/vtable could not be located -- the
// caller must treat this as a hard failure and skip installing the hook
// rather than dereferencing a null function pointer.
inline std::uintptr_t resolveVirtualSlot(const ModuleRanges& ranges, const char* mangledClassNameFragment, std::size_t slot){
	if(!ranges.found){ return 0; }
	auto occurrences = findAllInRange(ranges.segment1Start, ranges.segment1Len, mangledClassNameFragment);
	for(auto nameAddr : occurrences){
		auto typeinfoRef = findPtrInRange(ranges.segment2Start, ranges.segment2Len, nameAddr);
		if(!typeinfoRef){ continue; }
		auto typeinfo = typeinfoRef - sizeof(void*);
		auto vtableRef = findPtrInRange(ranges.segment2Start, ranges.segment2Len, typeinfo);
		if(!vtableRef){ continue; }
		auto vtable = vtableRef + sizeof(void*);
		auto slotPtr = reinterpret_cast<void* const*>(vtable + slot * sizeof(void*));
		if(*slotPtr){ return reinterpret_cast<std::uintptr_t>(*slotPtr); }
	}
	return 0;
}

} // namespace hf::Rtti
