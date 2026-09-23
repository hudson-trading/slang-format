//------------------------------------------------------------------------------
// FormatConfig.h
// Configuration options for slang-format
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <rfl/Description.hpp>
#include <rfl/config.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace format {

/// Default recursion budget for parsing and formatter syntax traversal.
inline constexpr uint32_t defaultMaxSyntaxDepth = 512;

/// Controls alignment padding and separation into groups.
struct AlignConfig {

    rfl::Description<
        "Alignment padding threshold in spaces. Split a group when adding a row "
        "would require this many or more spaces of padding; do not apply padding "
        "at or above the threshold. null disables the limit",
        std::optional<int>>
        paddingLimit = std::nullopt;

    rfl::Description<
        "Number of existing separator lines required to split an alignment "
        "group of assignment statements or standalone variable, net, and "
        "parameter declarations. Empty lines count once; a standalone N-line "
        "comment region counts N-1. Does not insert lines; values below 1 use 1.",
        int>
        statementGapLines = 1;

    rfl::Description<
        "Number of existing separator lines required to split an alignment "
        "group of ports, parameter ports, connections, case items, struct "
        "fields, assignment-pattern fields, and other non-statement rows. "
        "Empty lines count once; a standalone N-line comment region counts N-1. "
        "Does not insert lines; values below 1 use 1.",
        int>
        listGapLines = 2;
};

/// Configuration for slang-format
struct Config {
    rfl::Description<"Number of spaces per indentation level (0-64)", uint32_t> indentWidth = 4;

    rfl::Description<"Column limit for line wrapping (0 = no limit, maximum 1000000)", uint32_t>
        columnLimit = 100;

    rfl::Description<"Number of spaces before trailing comments (0-256)", uint32_t>
        spacesBeforeTrailingComment = 2;

    rfl::Description<
        "Maximum syntax tree depth and parser recursion budget (must be positive). "
        "Files exceeding the limit are skipped unchanged, even with --force. "
        "Raising this limit increases stack usage and can cause stack overflow",
        uint32_t>
        maxSyntaxDepth = defaultMaxSyntaxDepth;

    rfl::Description<"Alignment padding and group separation", AlignConfig> alignment = {};

    rfl::Description<
        "Exact directory names to skip during recursive file collection, at any depth. "
        "These are names, not paths or glob patterns. Explicit file and directory "
        "arguments are still processed",
        std::vector<std::string>>
        excludeDirectoryNames = std::vector<std::string>{};

    rfl::Description<
        "File or directory paths relative to the project root containing .slang/. "
        "When that root is targeted, collect files only from these paths; an empty "
        "list collects the whole tree. Ignored for explicit file or subdirectory "
        "targets, with --config or --config-json, or when configuration is found only via the "
        "current-directory fallback. Missing paths produce a warning",
        std::vector<std::string>>
        projectPaths = std::vector<std::string>{};
};

/// Find `.slang/format.json` by walking from startDir toward the filesystem root.
std::optional<std::filesystem::path> findConfigFile(const std::filesystem::path& startDir);

/// Load a formatter config file, returning a user-facing description on failure.
std::optional<Config> loadConfigFile(const std::filesystem::path& path, std::string& error);

/// Parse JSON with default values for missing settings and errors for unknown keys.
std::optional<Config> parseConfig(std::string_view json, std::string& error);

/// Reject invalid resource limits, overflowing layout arithmetic, and excessive padding.
/// Throws std::invalid_argument for both parsed and programmatically constructed configs.
void validateConfig(const Config& config);

} // namespace format
