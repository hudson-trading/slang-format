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

enum class AlignWrapStyle {
    /// Do not wrap alignments
    never,
    /// Wrap lines in the same group at the same point
    synced,
    /// Wrap lines independently
    independent
};

} // namespace format

namespace format {
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
        "group of in-body code (assignment statements, local "
        "declarations). Empty lines count once; a standalone N-line "
        "comment region counts N-1. Structural groups (ports, params, "
        "case items, struct members, struct-pattern assigns) always "
        "require 2 separator lines. Does not insert lines; values below 1 use 1.",
        int>
        groupSeparatorLines = 1;
};

/// Configuration for slang-format
struct Config {
    rfl::Description<"Number of spaces per indentation level", uint32_t> indentWidth = 4;

    rfl::Description<"Column limit for line wrapping (0 = no limit)", uint32_t> columnLimit = 100;

    rfl::Description<"Number of spaces before trailing comments", uint32_t>
        spacesBeforeTrailingComment = 2;

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
        "targets, with --config, or when configuration is found only via the "
        "current-directory fallback. Missing paths produce a warning",
        std::vector<std::string>>
        projectPaths = std::vector<std::string>{};
};

/// Find `.slang/format.json` by walking from startDir toward the filesystem root.
std::optional<std::filesystem::path> findConfigFile(const std::filesystem::path& startDir);

/// Load a formatter config file, returning a user-facing description on failure.
std::optional<Config> loadConfigFile(const std::filesystem::path& path, std::string& error);

} // namespace format
