//------------------------------------------------------------------------------
// formatter_main.cpp
// Entry point for slang-format SystemVerilog formatter
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------

#include "BS_thread_pool.hpp"
#include "format/FormatConfig.h"
#include "format/FormatValidation.h"
#include "format/Version.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fmt/color.h>
#include <fmt/format.h>
#include <iostream>
#include <iterator>
#include <rfl/json.hpp>
#include <string>
#include <vector>

#include "slang/util/CommandLine.h"
#include "slang/util/OS.h"
#include "slang/util/SmallVector.h"

using namespace slang;
using namespace slang::syntax;
namespace fs = std::filesystem;

namespace {

// Derive the directory to start config discovery from, given the positional
// targets. Uses the first target: its parent if it's a file, or the directory
// itself. Falls back to the current working directory when there are no
// targets (stdin mode) or the path can't be resolved.
fs::path configSearchRoot(const std::vector<std::string>& positional) {
    if (positional.empty())
        return fs::current_path();

    std::error_code ec;
    fs::path target = positional.front();
    bool isDir = fs::is_directory(target, ec);
    if (ec)
        return fs::current_path();
    return isDir ? target : target.parent_path();
}

// Recursively collect SystemVerilog source files under `dir`, skipping any
// subdirectory whose name exactly matches an entry in `excludeDirs`.
void collectSourceFiles(
    const fs::path& dir,
    const std::vector<std::string>& excludeDirs,
    std::vector<std::string>& out
) {
    static constexpr std::array<std::string_view, 4> kExtensions = {".sv", ".svh", ".v", ".vh"};

    std::error_code ec;
    fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec);
    if (ec)
        return;

    for (; it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec)
            break;

        const auto& entry = *it;
        if (entry.is_directory(ec)) {
            auto name = entry.path().filename().string();
            if (std::find(excludeDirs.begin(), excludeDirs.end(), name) != excludeDirs.end())
                it.disable_recursion_pending();
            continue;
        }
        if (!entry.is_regular_file(ec))
            continue;

        auto ext = entry.path().extension().string();
        if (std::find(kExtensions.begin(), kExtensions.end(), ext) == kExtensions.end())
            continue;

        out.push_back(entry.path().string());
    }
}

struct FileFormatResult {
    std::string path;
    std::string input;
    format::FormatResult result;
    bool fileReadError = false;
};
// Format a single file and return the result. May be threaded, so don't emit errors directly.
FileFormatResult formatFile(
    const std::string& path,
    const format::Config& config,
    format::FormatStage stage
) {
    FileFormatResult result;
    result.path = path;

    SmallVector<char> buffer;
    auto ec = OS::readFile(path, buffer);
    if (ec) {
        result.fileReadError = true;
        return result;
    }

    // OS::readFile appends a sentinel that is not part of the file.
    result.input.assign(buffer.data(), buffer.size() - 1);
    result.result = format::format(path, result.input, config, stage);
    return result;
}

// Render a colored `kind:` prefix, matching slang's diagnostic palette:
//   warnings -> bright_yellow, errors -> bright_red, file paths -> cyan.
// fmt::format with text_style emits ANSI escape codes unconditionally; we
// only call this when stderr is a TTY (the caller has already gated that
// via OS::setStderrColorsEnabled).
static std::string colorize(fmt::text_style style, std::string_view text) {
    return fmt::format(style, "{}", text);
}

// Print any diagnostics to stderr. Returns true if there were no diagnostics.
bool interpretResult(const format::FormatResult& result, std::string_view path) {
    auto diags = result.diagnostics();
    // Match slang's TextDiagnosticClient palette so the formatter's warnings
    // visually agree with parse-error lines that slang itself emits.
    const auto warnStyle = fmt::fg(fmt::terminal_color::bright_yellow) | fmt::emphasis::bold;
    const auto errStyle = fmt::fg(fmt::terminal_color::bright_red) | fmt::emphasis::bold;
    const auto pathStyle = fmt::fg(fmt::terminal_color::cyan);
    const bool useColor = OS::fileSupportsColors(stderr);

    auto kindPrefix = [&](std::string_view label, fmt::text_style style) {
        return useColor ? colorize(style, label) : std::string(label);
    };
    auto pathFmt = [&](std::string_view p) {
        if (!useColor)
            return fmt::format("'{}'", p);
        return colorize(pathStyle, fmt::format("'{}'", p));
    };

    for (auto& diag : diags) {
        // Parse errors: the diagnostic message already includes file locations
        // (slang renders them with `file:line:col:`), so we don't repeat the
        // path. CST/idempotency errors don't have those locations, so we put
        // the error kind first and then the file name so the kind is what
        // catches the eye when scanning a batch of warnings.
        switch (diag.kind) {
            case format::FormatDiagnosticKind::StructuralImbalance:
            case format::FormatDiagnosticKind::FailedReparse:
                OS::printE(fmt::format("{} {}\n", kindPrefix("warning:", warnStyle), diag.message));
                break;
            case format::FormatDiagnosticKind::CstMismatch:
                OS::printE(
                    fmt::format(
                        "{} {}\n  {}\n", kindPrefix("cst mismatch:", warnStyle), pathFmt(path),
                        diag.message
                    )
                );
                break;
            case format::FormatDiagnosticKind::NotIdempotent:
                OS::printE(
                    fmt::format(
                        "{} {}\n  {}\n", kindPrefix("not idempotent:", warnStyle), pathFmt(path),
                        diag.message
                    )
                );
                break;
            case format::FormatDiagnosticKind::InternalError:
            default:
                OS::printE(
                    fmt::format(
                        "{} {}: {}\n", kindPrefix("internal error:", errStyle), pathFmt(path),
                        diag.message
                    )
                );
                break;
        }
    }

    return diags.empty();
}

// Write formatted output to file
bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file << content;
    return file.good();
}

} // namespace

int main(int argc, char** argv) {
    OS::setupConsole();

    CommandLine cmdline;

    std::optional<bool> showHelp;
    cmdline.add("-h,--help", showHelp, "Display available options");

    std::optional<bool> showVersion;
    cmdline.add("--version", showVersion, "Display version information and exit");

    std::optional<bool> inplace;
    cmdline.add("-i,--inplace", inplace, "Edit files in-place");

    std::optional<bool> dryRun;
    cmdline.add("-n,--dry-run", dryRun, "Run formatting and validation but do not write any files");

    std::optional<bool> force;
    cmdline.add("-f,--force", force, "Force output even if validation fails");

    std::optional<std::string> configPath;
    cmdline.add(
        "--config", configPath, "Path to config file (default: .slang/format.json)", "<path>",
        CommandLineFlags::FilePath
    );

    std::optional<bool> dumpConfig;
    cmdline.add("--dump-config", dumpConfig, "Dump current configuration and exit");

    std::optional<std::string> stageName;
    cmdline.add("--stage", stageName, "Last formatter stage to run: layout or aligned", "<stage>");

    std::optional<uint32_t> numThreads;
    cmdline.add(
        "-j,--jobs", numThreads, "Number of parallel jobs (default: number of CPU cores)", "<n>"
    );

    std::optional<bool> verbose;
    cmdline.add("-v,--verbose", verbose, "Print each file before formatting (implies -j1)");

    std::vector<std::string> positional;
    cmdline.setPositional(positional, "files", CommandLineFlags::FilePath);

    if (!cmdline.parse(argc, argv)) {
        for (auto& err : cmdline.getErrors())
            OS::printE(fmt::format("{}\n", err.message));
        return 1;
    }

    format::FormatStage stage = format::FormatStage::Aligned;
    if (stageName) {
        if (*stageName == "layout")
            stage = format::FormatStage::Layout;
        else if (*stageName != "aligned") {
            OS::printE(
                fmt::format("error: invalid --stage '{}'; expected layout or aligned\n", *stageName)
            );
            return 1;
        }
    }

    if (showHelp == true) {
        OS::print(
            "OVERVIEW: slang-format - SystemVerilog formatter\n"
            "\n"
            "USAGE: slang-format [options] [<file-or-dir> ...]\n"
            "\n"
            "If no files are specified, reads from stdin and writes to stdout.\n"
            "If a single file is given without -i, prints formatted output to stdout.\n"
            "With -i, modifies files in-place.\n"
            "Directory arguments recurse into all .sv/.svh/.v/.vh files; subdirs are\n"
            "filtered using the config's excludeDirs (match on directory name).\n"
            "\n"
            "OPTIONS:\n"
            "  -h, --help        Display this help message\n"
            "  --version         Display version information\n"
            "  -i, --inplace     Edit files in-place\n"
            "  -n, --dry-run     Format and validate but do not write\n"
            "  -f, --force       Force output even if validation fails\n"
            "  --config <path>   Path to config file. If omitted, searches for\n"
            "                    .slang/format.json walking up from the target\n"
            "                    path, then from the current directory\n"
            "  --dump-config     Dump current configuration and exit\n"
            "  --stage <stage>   Stop after layout or aligned output (default: aligned)\n"
            "  -j, --jobs <n>    Number of parallel jobs (default: CPU cores)\n"
            "  -v, --verbose     Print each file before formatting (implies -j1)\n"
        );
        return 0;
    }

    if (showVersion == true) {
        OS::print(fmt::format("slang-format version {}\n", format::FullVersion));
        return 0;
    }

    // Load configuration
    format::Config config;

    // Directory containing the loaded `.slang/format.json` (the "config root"),
    // used to resolve the config's relative `dirs`. Empty when no config file
    // was found or an explicit --config was given (where `dirs` doesn't apply
    // because there is no implied project root to anchor it to).
    fs::path configRoot;

    if (configPath.has_value()) {
        // Use explicitly specified config file
        if (!fs::exists(*configPath)) {
            OS::printE(fmt::format("error: config file not found: '{}'\n", *configPath));
            return 1;
        }
        std::string error;
        auto loadedConfig = format::loadConfigFile(*configPath, error);
        if (!loadedConfig) {
            OS::printE(fmt::format("error: {}\n", error));
            return 1;
        }
        config = *loadedConfig;
    }
    else {
        // Discover `.slang/format.json`. First walk up from the target path
        // (the folder/file being formatted) so `slang-format /some/other/repo`
        // picks up that repo's config. If nothing is found there, fall back to
        // walking up from the current working directory — this matters when the
        // target is a throwaway temp file outside any project tree (for example,
        // when a lint runner formats a copied tempfile), where only the CWD
        // reflects the real project.
        //
        // `dirs` scoping only applies to a config found via the *target* tree
        // (configRoot set below). A CWD-fallback config is consulted for
        // formatting options but does not drive `dirs` — the target isn't its
        // config root.
        auto foundConfig = format::findConfigFile(configSearchRoot(positional));
        bool fromTarget = foundConfig.has_value();
        if (!foundConfig)
            foundConfig = format::findConfigFile(fs::current_path());

        if (foundConfig) {
            std::string error;
            auto loadedConfig = format::loadConfigFile(*foundConfig, error);
            if (!loadedConfig) {
                OS::printE(fmt::format("error: {}\n", error));
                return 1;
            }
            config = *loadedConfig;
            if (fromTarget) {
                // configRoot = directory holding `.slang/` (parent of `.slang`).
                std::error_code ec;
                configRoot = fs::weakly_canonical(foundConfig->parent_path().parent_path(), ec);
                if (ec)
                    configRoot = foundConfig->parent_path().parent_path();
            }
        }
        // If no config found, use defaults (already initialized)
    }

    // Dump config if requested
    if (dumpConfig == true) {
        OS::print(rfl::json::write(config, rfl::json::pretty));
        OS::print("\n");
        return 0;
    }

    auto outputResult = [&](const format::FormatResult& result, std::string_view input,
                            std::string_view path) {
        bool ok = interpretResult(result, path);
        bool skipped = !ok && !force.value_or(false) &&
                       (result.cstMismatch || result.notIdempotent || result.structuralImbalance) &&
                       result.internalError.empty() && !result.failedReparse;
        if (!ok && !skipped && !force.value_or(false))
            return 1;
        if (dryRun != true)
            OS::print(skipped ? input : std::string_view(result.formatted));
        return ok || skipped ? 0 : 1;
    };

    // If no files specified, read from stdin and write to stdout
    if (positional.empty()) {
        if (inplace == true) {
            OS::printE("error: -i cannot be used with stdin\n");
            return 1;
        }

        std::string input(std::istreambuf_iterator<char>(std::cin), {});
        return outputResult(format::format("stdin", input, config, stage), input, "<stdin>");
    }

    // Validate files exist. Directory args expand to all .sv/.svh/.v/.vh
    // files underneath, with subdirs filtered by the config's excludeDirs.
    // Files are added verbatim — only directory args are filtered, since
    // explicit file paths are an intentional opt-in.
    //
    // The config's `dirs` only applies when a directory arg IS the config root
    // (the directory holding `.slang/format.json`). In that case, formatting
    // is scoped to those subtrees instead of the whole tree. Pointing the
    // formatter at any other directory ignores `dirs` and crawls it directly,
    // so a subfolder can be formatted ad hoc without committing the whole tree.
    std::vector<std::string> files;
    for (const auto& path : positional) {
        if (!fs::exists(path)) {
            OS::printE(fmt::format("error: file not found: '{}'\n", path));
            return 1;
        }
        if (fs::is_directory(path)) {
            std::error_code ec;
            fs::path canonical = fs::weakly_canonical(path, ec);
            if (ec)
                canonical = path;
            bool isConfigRoot = !configRoot.empty() && !config.dirs.value().empty() &&
                                canonical == configRoot;
            if (isConfigRoot) {
                // Scope to the config's `dirs` (resolved relative to the
                // config root). A `dirs` entry that doesn't exist is skipped
                // with a warning rather than failing the whole run.
                for (const auto& d : config.dirs.value()) {
                    fs::path sub = configRoot / d;
                    if (!fs::exists(sub)) {
                        OS::printE(
                            fmt::format(
                                "warning: config 'dirs' entry not found: '{}'\n", sub.string()
                            )
                        );
                        continue;
                    }
                    if (fs::is_directory(sub))
                        collectSourceFiles(sub, config.excludeDirs.value(), files);
                    else
                        files.push_back(sub.string());
                }
            }
            else {
                collectSourceFiles(path, config.excludeDirs.value(), files);
            }
        }
        else {
            files.push_back(path);
        }
    }

    // Single file without -i: output to stdout
    if (files.size() == 1 && inplace != true) {
        auto result = formatFile(files[0], config, stage);
        if (result.fileReadError) {
            OS::printE(fmt::format("error: failed to read file '{}'\n", files[0]));
            return 1;
        }
        return outputResult(result.result, result.input, result.path);
    }

    // Multiple files require -i (or --dry-run, which doesn't write)
    if (files.size() > 1 && inplace != true && dryRun != true) {
        OS::printE("error: multiple files require -i or --dry-run\n");
        return 1;
    }

    // Format files with in-place editing
    const bool isVerbose = verbose.value_or(false);
    const uint32_t threads = isVerbose ? 1
                                       : numThreads.value_or(std::thread::hardware_concurrency());
    BS::thread_pool pool(threads);

    const bool isDryRun = dryRun.value_or(false);

    int errorCount = 0;
    int skippedCount = 0;
    int formattedCount = 0;
    int cstMismatchCount = 0;
    int idempotentFailCount = 0;

    auto processResult = [&](FileFormatResult& result) {
        if (result.result.excluded)
            return;

        if (result.fileReadError) {
            OS::printE(fmt::format("error: failed to read file '{}'\n", result.path));
            errorCount++;
            return;
        }

        bool ok = interpretResult(result.result, result.path);
        if (!ok) {
            if (result.result.cstMismatch)
                cstMismatchCount++;
            if (result.result.notIdempotent)
                idempotentFailCount++;
            if (!force.value_or(false)) {
                // Diagnostics caught ourselves before we wrote anything
                // harmful — count the file as skipped, not failed. The
                // summary line still tallies the CST/idempotency counts so
                // the user can see what was skipped, but we exit 0 (unless
                // a real failure like file-write or --force-with-errors
                // pushes errorCount up later).
                if (result.result.structuralImbalance || result.result.cstMismatch ||
                    result.result.notIdempotent) {
                    skippedCount++;
                }
                else {
                    errorCount++;
                }
                return;
            }
            // --force: user asked us to write even with bad output, so
            // anything that flags is a real error.
            errorCount++;
        }

        if (isDryRun) {
            // Count as "would format" without touching disk.
            formattedCount++;
            return;
        }

        if (!writeFile(result.path, result.result.formatted)) {
            OS::printE(fmt::format("error: failed to write '{}'\n", result.path));
            errorCount++;
        }
        else {
            formattedCount++;
        }
    };

    if (isVerbose) {
        // Verbose mode: format files sequentially with progress output
        for (const auto& path : files) {
            OS::printE(fmt::format("formatting {} ...\n", path));
            auto result = formatFile(path, config, stage);
            processResult(result);
        }
    }
    else {
        // Parallel mode: submit all files to thread pool
        std::vector<std::future<FileFormatResult>> futures;
        futures.reserve(files.size());

        for (const auto& path : files) {
            futures.push_back(pool.submit_task([&path, &config, stage]() {
                return formatFile(path, config, stage);
            }));
        }

        for (auto& future : futures) {
            auto result = future.get();
            processResult(result);
        }
    }

    OS::printE(fmt::format("{} {} files", isDryRun ? "would format" : "formatted", formattedCount));
    if (skippedCount > 0)
        OS::printE(fmt::format(", {} skipped (parse errors)", skippedCount));
    if (errorCount > 0)
        OS::printE(fmt::format(", {} errors", errorCount));
    if (cstMismatchCount > 0)
        OS::printE(fmt::format(" ({} CST mismatch)", cstMismatchCount));
    if (idempotentFailCount > 0)
        OS::printE(fmt::format(" ({} not idempotent)", idempotentFailCount));
    OS::printE("\n");

    return errorCount > 0 ? 1 : 0;
}
