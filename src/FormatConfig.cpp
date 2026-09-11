//------------------------------------------------------------------------------
// FormatConfig.cpp
// Configuration loading and discovery for slang-format
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------

#include "format/FormatConfig.h"

#include <fmt/format.h>
#include <fstream>
#include <rfl/DefaultIfMissing.hpp>
#include <rfl/NoExtraFields.hpp>
#include <rfl/json.hpp>
#include <sstream>

namespace fs = std::filesystem;

namespace format {

std::optional<fs::path> findConfigFile(const fs::path& startDir) {
    std::error_code ec;
    fs::path current = fs::weakly_canonical(startDir, ec);
    if (ec)
        current = startDir;

    while (true) {
        fs::path configPath = current / ".slang" / "format.json";
        if (fs::exists(configPath))
            return configPath;

        fs::path parent = current.parent_path();
        if (parent == current)
            break;
        current = parent;
    }

    return std::nullopt;
}

std::optional<Config> loadConfigFile(const fs::path& path, std::string& error) {
    error.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = fmt::format("could not read config file '{}'", path.string());
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    auto config = parseConfig(buffer.str(), error);
    if (!config)
        error = fmt::format("failed to parse config file '{}': {}", path.string(), error);
    return config;
}

std::optional<Config> parseConfig(std::string_view json, std::string& error) {
    error.clear();
    auto result = rfl::json::read<Config, rfl::DefaultIfMissing, rfl::NoExtraFields>(json);
    if (!result) {
        error = result.error().what();
        // The parser exposes only error text; translate its unknown-field advice
        // without losing nested field context or other errors in the same config.
        constexpr std::string_view prefix = "Value named '";
        constexpr std::string_view suffix =
            "' not used. Remove the rfl::NoExtraFields processor or add "
            "rfl::ExtraFields to avoid this error message.";
        constexpr std::string_view replacement = "Unknown configuration key '";
        for (size_t pos = 0; (pos = error.find(prefix, pos)) != std::string::npos;) {
            auto end = error.find(suffix, pos + prefix.size());
            if (end == std::string::npos)
                break;
            error.replace(end, suffix.size(), "'.");
            error.replace(pos, prefix.size(), replacement);
            pos = end + replacement.size() - prefix.size() + 2;
        }
        return std::nullopt;
    }

    return result.value();
}

} // namespace format
