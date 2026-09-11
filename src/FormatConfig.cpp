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
#include <limits>
#include <memory>
#include <rfl/DefaultIfMissing.hpp>
#include <rfl/NoExtraFields.hpp>
#include <rfl/json.hpp>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace format {

void validateConfig(const Config& config) {
    auto check = [](std::string_view name, uint32_t value, uint32_t maximum) {
        if (value > maximum)
            throw std::invalid_argument(fmt::format("{} must be between 0 and {}", name, maximum));
    };
    check("indentWidth", config.indentWidth.get(), 64);
    check("columnLimit", config.columnLimit.get(), 1000000);
    check("spacesBeforeTrailingComment", config.spacesBeforeTrailingComment.get(), 256);
}

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

    // reflect-cpp narrows JSON integers before constructing the config fields.
    // Inspect the original numbers too, so wrapped values cannot pass validation.
    std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> document(
        yyjson_read(json.data(), json.size(), 0), yyjson_doc_free
    );
    if (!document) {
        error = "failed to read configuration JSON";
        return std::nullopt;
    }
    auto root = yyjson_doc_get_root(document.get());
    auto fits = [&](yyjson_val* object, const char* key, bool unsignedValue) {
        auto value = yyjson_obj_get(object, key);
        if (!value || yyjson_is_null(value))
            return true;
        bool valid = unsignedValue
                         ? yyjson_is_uint(value) &&
                               yyjson_get_uint(value) <= std::numeric_limits<uint32_t>::max()
                         : yyjson_is_int(value) &&
                               (!yyjson_is_uint(value) ||
                                yyjson_get_uint(value) <= std::numeric_limits<int>::max()) &&
                               yyjson_get_sint(value) >= std::numeric_limits<int>::min() &&
                               yyjson_get_sint(value) <= std::numeric_limits<int>::max();
        if (!valid)
            error = fmt::format("configuration value '{}' is outside its integer range", key);
        return valid;
    };
    if (!fits(root, "indentWidth", true) || !fits(root, "columnLimit", true) ||
        !fits(root, "spacesBeforeTrailingComment", true))
        return std::nullopt;
    if (auto alignment = yyjson_obj_get(root, "alignment");
        alignment &&
        (!fits(alignment, "paddingLimit", false) || !fits(alignment, "groupSeparatorLines", false)))
        return std::nullopt;
    try {
        validateConfig(result.value());
    }
    catch (const std::invalid_argument& e) {
        error = e.what();
        return std::nullopt;
    }
    return result.value();
}

} // namespace format
