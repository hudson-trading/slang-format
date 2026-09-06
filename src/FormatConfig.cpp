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
    auto result = rfl::json::read<Config, rfl::DefaultIfMissing>(buffer.str());
    if (!result) {
        error = fmt::format(
            "failed to parse config file '{}': {}", path.string(), result.error().what()
        );
        return std::nullopt;
    }

    return result.value();
}

} // namespace format
