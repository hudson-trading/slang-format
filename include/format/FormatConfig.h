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
struct AlignConfig {

    rfl::Description<"Split the group if a space of this size or more is inserted "
                     "(null = never split on padding width)",
                     std::optional<int>>
        maxSpaces = std::nullopt;

    rfl::Description<"Number of separator lines required to split an alignment "
                     "group of in-body code (assignment statements, local "
                     "declarations). Empty lines count once; a standalone N-line "
                     "comment region counts N-1. Structural groups (ports, params, "
                     "case items, struct members, struct-pattern assigns) always "
                     "require 2 separator lines.",
                     int>
        linesBetweenGroups = 1;
};

/// Configuration for slang-format
struct Config {
    rfl::Description<"Number of spaces per indentation level", uint32_t> indentWidth = 4;

    rfl::Description<"Column limit for line wrapping (0 = no limit)", uint32_t> columnLimit = 100;

    rfl::Description<"Number of spaces before trailing comments", uint32_t>
        spacesBeforeTrailingComment = 2;

    rfl::Description<"When true, preserve user line breaks in expressions if all resulting lines "
                     "fit within the column limit",
                     bool>
        respectUserFormatting = false;

    rfl::Description<"Alignment config", AlignConfig> alignment = {};

    rfl::Description<"Directory names to exclude from formatting, omitted while crawling",
                     std::vector<std::string>>
        excludeDirs = std::vector<std::string>{};

    rfl::Description<"Paths (relative to the config file's directory) to format when the "
                     "formatter is pointed at that config directory itself. Lets a project "
                     "scope formatting to specific subtrees (e.g. [\"fpga\"]) without "
                     "formatting the whole tree. Ignored when a subdirectory or file is "
                     "targeted directly, so you can still format any path ad hoc.",
                     std::vector<std::string>>
        dirs = std::vector<std::string>{};
};

/// Find `.slang/format.json` by walking from startDir toward the filesystem root.
std::optional<std::filesystem::path> findConfigFile(const std::filesystem::path& startDir);

/// Load a formatter config file, returning a user-facing description on failure.
std::optional<Config> loadConfigFile(const std::filesystem::path& path, std::string& error);

} // namespace format
