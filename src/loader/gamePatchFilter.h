#ifndef KYTY_LOADER_GAME_PATCH_FILTER_H_
#define KYTY_LOADER_GAME_PATCH_FILTER_H_

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace Loader::GamePatch {

// KYTY_GAME_PATCH_DISABLE=<name substring>[;<name substring>...] skips the matching mods of the
// --game-patch file when it is loaded, without editing the file. A mod matches when a pattern is
// a case-insensitive substring of its "name". ';' and '|' both separate patterns ('|' because
// Start-Comparison -ExtraEnv values cannot contain ';'). Blank patterns are ignored, so an unset
// or empty variable changes nothing.
inline constexpr const char* kDisableEnv = "KYTY_GAME_PATCH_DISABLE";

inline char LowerAscii(char c) {
	return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

inline std::vector<std::string> ParseDisableList(std::string_view text) {
	std::vector<std::string> patterns;
	size_t                   begin = 0;
	while (begin <= text.size()) {
		size_t end = text.find_first_of(";|", begin);
		if (end == std::string_view::npos) {
			end = text.size();
		}
		auto piece = text.substr(begin, end - begin);
		while (!piece.empty() && (piece.front() == ' ' || piece.front() == '\t')) {
			piece.remove_prefix(1);
		}
		while (!piece.empty() && (piece.back() == ' ' || piece.back() == '\t' ||
		                          piece.back() == '\r' || piece.back() == '\n')) {
			piece.remove_suffix(1);
		}
		if (!piece.empty()) {
			patterns.emplace_back(piece);
		}
		begin = end + 1;
	}
	return patterns;
}

inline bool ContainsNoCase(std::string_view text, std::string_view pattern) {
	if (pattern.empty() || pattern.size() > text.size()) {
		return false;
	}
	for (size_t start = 0; start + pattern.size() <= text.size(); start++) {
		size_t index = 0;
		while (index < pattern.size() &&
		       LowerAscii(text[start + index]) == LowerAscii(pattern[index])) {
			index++;
		}
		if (index == pattern.size()) {
			return true;
		}
	}
	return false;
}

// Index of the first pattern that disables the mod, or -1.
inline int FindDisablingPattern(std::string_view                mod_name,
                                const std::vector<std::string>& patterns) {
	for (size_t index = 0; index < patterns.size(); index++) {
		if (ContainsNoCase(mod_name, patterns[index])) {
			return static_cast<int>(index);
		}
	}
	return -1;
}

enum class ModDecision { Apply, SkipDisabledInFile, SkipDisabledByEnv };

struct ModEntry {
	std::string name;
	bool        enabled = true;
};

struct ModSelection {
	std::vector<ModDecision> decisions;       // one per mod, in file order
	std::vector<int>         pattern_of_mod;  // disabling pattern of SkipDisabledByEnv mods, else -1
	std::vector<bool>        pattern_matched; // one per pattern: it matched at least one mod name
};

// A mod the file itself disables stays skipped whatever the patterns say; patterns only remove
// mods. A pattern that matches no mod name at all is reported by pattern_matched, so a typo is
// visible in the log instead of silently applying the mod.
inline ModSelection SelectMods(const std::vector<ModEntry>&    mods,
                               const std::vector<std::string>& patterns) {
	ModSelection selection;
	selection.decisions.reserve(mods.size());
	selection.pattern_of_mod.reserve(mods.size());
	selection.pattern_matched.assign(patterns.size(), false);
	for (const auto& mod: mods) {
		for (size_t index = 0; index < patterns.size(); index++) {
			if (ContainsNoCase(mod.name, patterns[index])) {
				selection.pattern_matched[index] = true;
			}
		}
		const int pattern = FindDisablingPattern(mod.name, patterns);
		if (!mod.enabled) {
			selection.decisions.push_back(ModDecision::SkipDisabledInFile);
			selection.pattern_of_mod.push_back(-1);
		} else if (pattern >= 0) {
			selection.decisions.push_back(ModDecision::SkipDisabledByEnv);
			selection.pattern_of_mod.push_back(pattern);
		} else {
			selection.decisions.push_back(ModDecision::Apply);
			selection.pattern_of_mod.push_back(-1);
		}
	}
	return selection;
}

// Astro Bot draw-thread command buffer table (KYTY_ASTRO_CB_SCALE): five 32-bit sizes in MiB.
// ApplyBuiltin patches it at a fixed address in the verified PPSA21567 01.018.000 build; other
// Astro Bot builds can opt in to a signature scan that must find exactly one copy of the table.
inline constexpr uint32_t kAstroCbSizes[5] = {7, 7, 4, 4, 6};

// Title IDs of Astro Bot builds this fork has been tested with: PPSA21567 (01.018.000, the table
// address is verified) and PPSA21564 (01.007.000, the tools/astro-bot kit's pinned game).
inline bool IsAstroBotTitle(std::string_view title_id) {
	return title_id == "PPSA21567" || title_id == "PPSA21564";
}

// Counts the occurrences of `needle` in `data` (overlapping ones too), stopping at `limit`, and
// stores the offset of the first one in `first`. A table is patched only when the total count over
// every scanned segment is exactly one.
inline size_t CountOccurrences(const uint8_t* data, size_t size, const void* needle,
                               size_t needle_size, size_t limit, size_t* first) {
	size_t count = 0;
	if (data == nullptr || needle == nullptr || needle_size == 0 || size < needle_size) {
		return 0;
	}
	const auto* bytes = static_cast<const uint8_t*>(needle);
	for (size_t offset = 0; offset + needle_size <= size && count < limit; offset++) {
		if (data[offset] == bytes[0] && std::memcmp(data + offset, bytes, needle_size) == 0) {
			if (count == 0 && first != nullptr) {
				*first = offset;
			}
			count++;
		}
	}
	return count;
}

// KYTY_ASTRO_CB_SCALE: an unset value means the default scale (2) for the verified build only; a
// set value is clamped to 1-4 and also opts other Astro Bot builds in to the signature scan.
struct AstroCbScale {
	int  scale    = 2;
	bool explicit_value = false;
};

inline AstroCbScale ParseAstroCbScale(const char* value) {
	AstroCbScale result;
	if (value == nullptr || *value == '\0') {
		return result;
	}
	char* end    = nullptr;
	long  parsed = std::strtol(value, &end, 10);
	if (end == value) {
		return result;
	}
	result.scale          = static_cast<int>(parsed < 1 ? 1 : (parsed > 4 ? 4 : parsed));
	result.explicit_value = true;
	return result;
}

} // namespace Loader::GamePatch

#endif // KYTY_LOADER_GAME_PATCH_FILTER_H_
