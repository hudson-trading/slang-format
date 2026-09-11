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
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fmt/color.h>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <random>
#include <rfl/json.hpp>
#include <string>
#include <vector>

#include "slang/util/CommandLine.h"
#include "slang/util/OS.h"
#include "slang/util/ScopeGuard.h"
#include "slang/util/SmallVector.h"

#if defined(_WIN32)
#    include <fcntl.h>
#    include <io.h>
#endif

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
    return !ec && isDir ? target : target.parent_path();
}

// Recursively collect SystemVerilog source files under `dir`, skipping any
// subdirectory whose name exactly matches an entry in `excludeDirectoryNames`.
void collectSourceFiles(
    const fs::path& dir,
    const std::vector<std::string>& excludeDirectoryNames,
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
            if (std::find(excludeDirectoryNames.begin(), excludeDirectoryNames.end(), name) !=
                excludeDirectoryNames.end())
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

enum class FileFormatStatus { Changed, Unchanged, Excluded, Skipped, Error };

struct FileFormatResult {
    std::string path;
    std::string input;
    format::FormatResult result;
    std::string error;
    double elapsedMs = 0;
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
        result.error = fmt::format("failed to read file '{}': {}", path, ec.message());
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

// Print any diagnostics to stderr.
void printDiagnostics(const format::FormatResult& result, std::string_view path) {
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

    for (const auto& diag : result.diagnostics) {
        auto location = diag.line ? fmt::format("{}:{}", path, *diag.line) : std::string(path);
        // Parse errors: the diagnostic message already includes file locations
        // (slang renders them with `file:line:col:`), so we don't repeat the
        // path. CST/idempotency errors don't have those locations, so we put
        // the error kind first and then the file name so the kind is what
        // catches the eye when scanning a batch of warnings.
        switch (diag.kind) {
            case format::FormatDiagnosticKind::UnmatchedFormatOn:
                OS::printE(
                    fmt::format(
                        "{} {}: {}\n", kindPrefix("warning:", warnStyle), pathFmt(path),
                        diag.message
                    )
                );
                break;
            case format::FormatDiagnosticKind::MergeConflict:
                OS::printE(
                    fmt::format(
                        "{} {}: {}\n", kindPrefix("error:", errStyle), pathFmt(location),
                        diag.message
                    )
                );
                break;
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
}

// Replace a completed file beside its destination, preserving symlink targets and permissions.
bool writeFile(const std::string& path, const std::string& content, std::string& error) {
    std::error_code ec;
    auto destination = fs::canonical(path, ec);
    if (ec) {
        error = ec.message();
        return false;
    }
    auto permissions = fs::status(destination, ec).permissions();
    if (ec) {
        error = ec.message();
        return false;
    }
    auto writable = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;
    if ((permissions & writable) == fs::perms::none) {
        error = "file is read-only";
        return false;
    }

    struct TemporaryFile {
        fs::path path;
        std::FILE* stream = nullptr;
        ~TemporaryFile() {
            if (stream)
                std::fclose(stream);
            if (!path.empty()) {
                std::error_code ignored;
                fs::remove(path, ignored);
            }
        }
    } temporary;
    std::random_device random;
    for (int attempt = 0; attempt < 100; attempt++) {
        auto candidate = destination.parent_path() /
                         fmt::format(".slang-format-{}-{}.tmp", OS::getpid(), random());
#if defined(_WIN32)
        auto stream = _wfopen(candidate.c_str(), L"wbx");
#else
        auto stream = std::fopen(candidate.c_str(), "wbx");
#endif
        if (stream) {
            temporary.path = std::move(candidate);
            temporary.stream = stream;
            break;
        }
        if (errno != EEXIST) {
            error = std::error_code(errno, std::generic_category()).message();
            return false;
        }
    }
    if (!temporary.stream) {
        error = "could not create a temporary file";
        return false;
    }
    bool written = std::fwrite(content.data(), 1, content.size(), temporary.stream) ==
                   content.size();
    int closed = std::fclose(temporary.stream);
    temporary.stream = nullptr;
    if (!written || closed != 0) {
        error = "could not complete temporary file write";
        return false;
    }
    fs::permissions(temporary.path, permissions, ec);
    if (!ec)
        fs::rename(temporary.path, destination, ec);
    if (ec) {
        error = ec.message();
        return false;
    }
    temporary.path.clear();
    return true;
}

} // namespace

int main(int argc, char** argv) {
    OS::setupConsole();
#if defined(_WIN32)
    // Source text can contain preserved CRLF sequences. Disable CRT newline
    // translation so stdin and stdout remain byte-for-byte lossless.
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    CommandLine cmdline;

    std::optional<bool> showHelp;
    cmdline.add("-h,--help", showHelp, "Display available options");

    std::optional<bool> showVersion;
    cmdline.add("--version", showVersion, "Display version information and exit");

    std::optional<bool> inplace;
    cmdline.add("-i,--inplace", inplace, "Edit files in-place");

    std::optional<bool> dryRun;
    cmdline.add("-n,--dry-run", dryRun, "Run formatting and validation but do not write any files");

    std::optional<bool> check;
    cmdline.add(
        "--check,--verify", check, "Fail if formatting changes or validation fails; do not write"
    );

    std::optional<bool> warningsAsErrors;
    cmdline.add(
        "--Werror", warningsAsErrors, "Treat warnings and dry-run formatting changes as errors"
    );

    std::optional<bool> strict;
    cmdline.add("--strict,--fail-on-incomplete-format", strict, "Fail on any validation failure");

    std::optional<bool> failsafeSuccess;
    cmdline.add(
        "--failsafe_success", failsafeSuccess, "Allow skipped validation failures (default: true)"
    );

    std::optional<bool> force;
    cmdline.add("-f,--force", force, "Force output even if validation fails");

    std::optional<std::string> configPath;
    cmdline.add(
        "--config", configPath, "Path to config file (default: .slang/format.json)", "<path>",
        CommandLineFlags::FilePath
    );

    std::optional<std::string> configJson;
    cmdline.add("--config-json", configJson, "Inline JSON configuration", "<json>");

    std::optional<std::string> assumeFilename;
    cmdline.add(
        "--assume-filename,--stdin_name", assumeFilename,
        "Filename for stdin configuration discovery and diagnostics", "<path>"
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
    cmdline.add("-v,--verbose", verbose, "Report file starts, finishes, and elapsed milliseconds");

    std::optional<std::string> statsPath;
    cmdline.add(
        "--stats-csv", statsPath, "Write per-file timing and outcomes to CSV", "<path>",
        CommandLineFlags::FilePath
    );

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
            "With no files or a single -, reads stdin and writes stdout.\n"
            "If a single file is given without -i, prints formatted output to stdout.\n"
            "With -i, modifies files in-place.\n"
            "Directory arguments recurse into all .sv/.svh/.v/.vh files; subdirs are\n"
            "filtered using the config's excludeDirectoryNames (match on directory name).\n"
            "\n"
            "OPTIONS:\n"
            "  -h, --help        Display this help message\n"
            "  --version         Display version information\n"
            "  -i, --inplace     Edit files in-place\n"
            "  -n, --dry-run     Format and validate but do not write\n"
            "  --check, --verify Check formatting and validation without writing\n"
            "  --Werror         Fail on warnings and dry-run formatting changes\n"
            "  --strict         Fail on any validation failure without forcing output\n"
            "  --fail-on-incomplete-format  Alias for --strict\n"
            "  --failsafe_success=false    Alias for --strict\n"
            "  -f, --force       Force output even if validation fails\n"
            "  --config <path>   Path to config file. Without an explicit config, searches for\n"
            "                    .slang/format.json walking up from the target\n"
            "                    path, then from the current directory\n"
            "  --config-json <json>  Inline JSON config; cannot combine with --config\n"
            "                    Explicit configs use defaults for omitted settings\n"
            "  --assume-filename <path>  Use this stdin path for config and diagnostics\n"
            "  --stdin_name <path>      Alias for --assume-filename\n"
            "  --dump-config     Dump current configuration and exit\n"
            "  --stage <stage>   Stop after layout or aligned output (default: aligned)\n"
            "  -j, --jobs <n>    Number of parallel jobs (default: CPU cores)\n"
            "  -v, --verbose     Report file starts, finishes, and elapsed milliseconds\n"
            "  --stats-csv <path>  Write per-file timing and outcomes to CSV\n"
        );
        return 0;
    }

    if (showVersion == true) {
        OS::print(fmt::format("slang-format version {}\n", format::FullVersion));
        return 0;
    }

    if (configJson && configPath) {
        OS::printE("error: --config and --config-json cannot be combined\n");
        return 1;
    }
    std::optional<format::Config> inlineConfig;
    if (configJson) {
        std::string error;
        inlineConfig = format::parseConfig(*configJson, error);
        if (!inlineConfig) {
            OS::printE(fmt::format("error: failed to parse --config-json: {}\n", error));
            return 1;
        }
    }

    const bool readStdin = positional.empty() || (positional.size() == 1 && positional[0] == "-");
    if (!readStdin && std::ranges::find(positional, "-") != positional.end()) {
        OS::printE("error: stdin (-) cannot be combined with other inputs\n");
        return 1;
    }
    if (readStdin && inplace == true) {
        OS::printE("error: -i cannot be used with stdin\n");
        return 1;
    }
    const std::string stdinName = assumeFilename.value_or("<stdin>");
    auto configTargets = readStdin ? std::vector<std::string>{} : positional;
    if (readStdin && assumeFilename)
        configTargets.push_back(*assumeFilename);

    const bool checkFormatting = check == true || (dryRun == true && warningsAsErrors == true);
    const bool noWrite = dryRun == true || check == true;
    const bool strictValidation = strict == true || failsafeSuccess == false || checkFormatting;

    // Resolution happens on the main thread; workers receive independent configs.
    struct ResolvedConfig {
        format::Config config;
        fs::path root;
        std::string error;
    };
    std::map<fs::path, ResolvedConfig> configCache;
    auto resolveConfig = [&](const fs::path& directory) {
        ResolvedConfig resolved;
        if (inlineConfig) {
            resolved.config = *inlineConfig;
            return resolved;
        }
        auto found = configPath ? std::optional<fs::path>(*configPath)
                                : format::findConfigFile(directory);
        if (found && !configPath)
            resolved.root = fs::weakly_canonical(found->parent_path().parent_path());
        if (!found)
            found = format::findConfigFile(fs::current_path());
        if (!found)
            return resolved;

        auto key = fs::weakly_canonical(*found);
        auto [it, inserted] = configCache.try_emplace(key);
        if (inserted) {
            auto loaded = format::loadConfigFile(*found, it->second.error);
            if (loaded)
                it->second.config = *loaded;
        }
        resolved.config = it->second.config;
        resolved.error = it->second.error;
        return resolved;
    };

    if (dumpConfig == true || readStdin) {
        auto resolved = resolveConfig(configSearchRoot(configTargets));
        if (!resolved.error.empty()) {
            OS::printE(fmt::format("error: {}\n", resolved.error));
            return 1;
        }
        if (dumpConfig == true) {
            OS::print(rfl::json::write(resolved.config, rfl::json::pretty));
            OS::print("\n");
            return 0;
        }
    }

    std::ofstream stats;
    bool statsFailed = false;
    auto statsError = [&] {
        if (!statsFailed)
            OS::printE(fmt::format("error: failed to write statistics '{}'\n", *statsPath));
        statsFailed = true;
    };
    auto openStats = [&](const std::vector<std::string>& inputs) {
        if (!statsPath)
            return true;
        if (*statsPath == "-") {
            OS::printE("error: --stats-csv requires a file path, not stdout (-)\n");
            return false;
        }
        std::error_code ec;
        auto destination = fs::weakly_canonical(*statsPath, ec);
        if (ec) {
            statsError();
            return false;
        }
        auto conflicts = [&](const fs::path& input) {
            std::error_code ignored;
            if (fs::weakly_canonical(input, ignored) == destination && !ignored)
                return true;
            return fs::equivalent(input, destination, ignored) && !ignored;
        };
        for (const auto& input : inputs) {
            if (conflicts(input)) {
                OS::printE("error: --stats-csv must not overwrite an input file\n");
                return false;
            }
        }
        for (const auto& [path, resolved] : configCache) {
            if (conflicts(path)) {
                OS::printE("error: --stats-csv must not overwrite a config file\n");
                return false;
            }
        }
        stats.open(*statsPath, std::ios::binary | std::ios::trunc);
        stats << "path,elapsed_ms,input_bytes,status,validation_failed\n";
        stats.flush();
        if (!stats) {
            statsError();
            return false;
        }
        return true;
    };
    auto writeStats = [&](const FileFormatResult& result, FileFormatStatus status) {
        if (!statsPath || statsFailed)
            return;
        std::string_view statusName;
        switch (status) {
            case FileFormatStatus::Changed:
                statusName = "changed";
                break;
            case FileFormatStatus::Unchanged:
                statusName = "unchanged";
                break;
            case FileFormatStatus::Excluded:
                statusName = "excluded";
                break;
            case FileFormatStatus::Skipped:
                statusName = "skipped";
                break;
            case FileFormatStatus::Error:
                statusName = "error";
                break;
        }
        // Always quote paths so commas, quotes, and newlines round-trip through CSV readers.
        stats.put('"');
        for (char ch : result.path) {
            if (ch == '"')
                stats.put('"');
            stats.put(ch);
        }
        stats << fmt::format(
            "\",{:.3f},{},{},{}\n", result.elapsedMs, result.input.size(), statusName,
            result.result.isUsable() ? "false" : "true"
        );
        stats.flush();
        if (!stats)
            statsError();
    };
    auto closeStats = [&] {
        if (stats.is_open()) {
            stats.close();
            if (!stats)
                statsError();
        }
        return statsFailed;
    };
    auto finishVerbose = [&](const FileFormatResult& result) {
        OS::printE(fmt::format("finished {} ({:.3f} ms)\n", result.path, result.elapsedMs));
    };
    auto outputResult = [&](const FileFormatResult& file) {
        const auto& result = file.result;
        auto action = result.outputAction(force.value_or(false));
        int status = 1;
        FileFormatStatus outcome = FileFormatStatus::Error;
        if (!file.error.empty()) {
            OS::printE(fmt::format("error: {}\n", file.error));
        }
        else {
            printDiagnostics(result, file.path);
            if (action != format::FormatOutputAction::Abort) {
                bool skipped = action == format::FormatOutputAction::KeepOriginal;
                bool changed = !skipped && result.formatted != file.input;
                outcome = result.generated ? FileFormatStatus::Excluded
                          : skipped        ? FileFormatStatus::Skipped
                          : changed        ? FileFormatStatus::Changed
                                           : FileFormatStatus::Unchanged;
                if (noWrite && changed)
                    OS::printE(fmt::format("{}: needs formatting\n", file.path));
                if (!noWrite)
                    OS::print(
                        skipped ? std::string_view(file.input) : std::string_view(result.formatted)
                    );
                bool failed = (!result.isUsable() && (!skipped || strictValidation)) ||
                              (warningsAsErrors == true && !result.diagnostics.empty());
                status = failed || (checkFormatting && changed) ? 1 : 0;
            }
        }
        writeStats(file, outcome);
        if (verbose == true)
            finishVerbose(file);
        return closeStats() ? 1 : status;
    };

    if (readStdin) {
        if (!openStats(
                assumeFilename ? std::vector<std::string>{*assumeFilename}
                               : std::vector<std::string>{}
            ))
            return 1;
        if (verbose == true)
            OS::printE(fmt::format("formatting {} ...\n", stdinName));
        auto resolved = resolveConfig(configSearchRoot(configTargets));
        auto start = std::chrono::steady_clock::now();
        FileFormatResult file;
        file.path = stdinName;
        file.input.assign(std::istreambuf_iterator<char>(std::cin), {});
        file.result = format::format(stdinName, file.input, resolved.config, stage);
        file.elapsedMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        return outputResult(file);
    }

    // Validate files exist. Directory args expand to all .sv/.svh/.v/.vh
    // files underneath, with subdirs filtered by the config's excludeDirectoryNames.
    // Files are added verbatim — only directory args are filtered, since
    // explicit file paths are an intentional opt-in.
    //
    // The config's `projectPaths` only applies when a directory arg IS the config root
    // (the directory holding `.slang/format.json`). In that case, formatting
    // is scoped to those subtrees instead of the whole tree. Pointing the
    // formatter at any other directory ignores `projectPaths` and crawls it directly,
    // so a subfolder can be formatted ad hoc without committing the whole tree.
    std::vector<std::string> files;
    for (const auto& path : positional) {
        if (!fs::exists(path)) {
            OS::printE(fmt::format("error: file not found: '{}'\n", path));
            return 1;
        }
        if (fs::is_directory(path)) {
            auto resolved = resolveConfig(path);
            if (!resolved.error.empty()) {
                OS::printE(fmt::format("error: {}\n", resolved.error));
                return 1;
            }
            const auto& config = resolved.config;
            const auto& configRoot = resolved.root;
            std::error_code ec;
            fs::path canonical = fs::weakly_canonical(path, ec);
            if (ec)
                canonical = path;
            bool isConfigRoot = !configRoot.empty() && !config.projectPaths.value().empty() &&
                                canonical == configRoot;
            if (isConfigRoot) {
                // Scope to the config's `projectPaths` (resolved relative to the
                // config root). A `projectPaths` entry that doesn't exist is skipped
                // with a warning rather than failing the whole run.
                for (const auto& d : config.projectPaths.value()) {
                    fs::path sub = configRoot / d;
                    if (!fs::exists(sub)) {
                        OS::printE(
                            fmt::format(
                                "warning: config 'projectPaths' entry not found: '{}'\n",
                                sub.string()
                            )
                        );
                        continue;
                    }
                    if (fs::is_directory(sub))
                        collectSourceFiles(sub, config.excludeDirectoryNames.value(), files);
                    else
                        files.push_back(sub.string());
                }
            }
            else {
                collectSourceFiles(path, config.excludeDirectoryNames.value(), files);
            }
        }
        else {
            files.push_back(path);
        }
    }

    std::vector<ResolvedConfig> fileConfigs;
    fileConfigs.reserve(files.size());
    for (const auto& path : files)
        fileConfigs.push_back(resolveConfig(fs::path(path).parent_path()));
    auto formatInput = [&](size_t index) {
        auto start = std::chrono::steady_clock::now();
        FileFormatResult result;
        if (!fileConfigs[index].error.empty()) {
            result.path = files[index];
            result.error = fileConfigs[index].error;
        }
        else {
            result = formatFile(files[index], fileConfigs[index].config, stage);
        }
        result.elapsedMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        return result;
    };

    if (files.size() > 1 && inplace != true && !noWrite) {
        OS::printE("error: multiple files require -i, --dry-run, or --check\n");
        return 1;
    }
    if (!openStats(files))
        return 1;

    // Single file without -i: output to stdout.
    if (files.size() == 1 && inplace != true) {
        if (verbose == true)
            OS::printE(fmt::format("formatting {} ...\n", files[0]));
        return outputResult(formatInput(0));
    }

    // Format files with in-place editing
    const bool isVerbose = verbose.value_or(false);
    const bool reportCompletions = isVerbose || statsPath.has_value();
    const uint32_t threads = numThreads.value_or(std::thread::hardware_concurrency());
    struct ProgressEvent {
        size_t index;
        bool finished;
    };
    std::mutex progressMutex;
    std::condition_variable progressReady;
    std::deque<ProgressEvent> progress;
    auto reportProgress = [&](size_t index, bool finished) {
        {
            std::lock_guard lock(progressMutex);
            progress.push_back({index, finished});
        }
        progressReady.notify_one();
    };
    // The pool joins its workers before their captured progress state is destroyed.
    BS::thread_pool pool(threads);

    const bool isDryRun = noWrite;
    bool checkFailed = false;

    int errorCount = 0;
    int skippedCount = 0;
    int formattedCount = 0;
    int unchangedCount = 0;
    int excludedCount = 0;
    int failedCount = 0;
    int cstMismatchCount = 0;
    int idempotentFailCount = 0;

    auto processResult = [&](FileFormatResult& result) -> FileFormatStatus {
        if (!result.error.empty()) {
            OS::printE(fmt::format("error: {}\n", result.error));
            errorCount++;
            failedCount++;
            return FileFormatStatus::Error;
        }

        printDiagnostics(result.result, result.path);
        if ((strictValidation && !result.result.isUsable()) ||
            (warningsAsErrors == true && !result.result.diagnostics.empty()))
            checkFailed = true;
        if (result.result.hasDiagnostic(format::FormatDiagnosticKind::CstMismatch))
            cstMismatchCount++;
        if (result.result.hasDiagnostic(format::FormatDiagnosticKind::NotIdempotent))
            idempotentFailCount++;

        if (!result.result.isUsable())
            failedCount++;
        auto action = result.result.outputAction(force.value_or(false));
        if (action == format::FormatOutputAction::Abort) {
            errorCount++;
            return FileFormatStatus::Error;
        }
        if (action == format::FormatOutputAction::KeepOriginal) {
            if (!result.result.generated)
                skippedCount++;
            else
                excludedCount++;
            return result.result.generated ? FileFormatStatus::Excluded : FileFormatStatus::Skipped;
        }
        // Forced output with validation failures still returns an error status.
        if (!result.result.isUsable())
            errorCount++;

        if (result.result.formatted == result.input) {
            unchangedCount++;
            return FileFormatStatus::Unchanged;
        }
        if (isDryRun) {
            if (result.result.formatted != result.input) {
                OS::printE(fmt::format("{}: needs formatting\n", result.path));
                checkFailed = checkFailed || checkFormatting;
            }
            // Count successful formatting attempts without touching disk.
            formattedCount++;
            return FileFormatStatus::Changed;
        }

        std::string writeError;
        if (!writeFile(result.path, result.result.formatted, writeError)) {
            OS::printE(fmt::format("error: failed to write '{}': {}\n", result.path, writeError));
            errorCount++;
            if (result.result.isUsable())
                failedCount++;
            return FileFormatStatus::Error;
        }
        else {
            formattedCount++;
        }
        return FileFormatStatus::Changed;
    };

    std::vector<std::future<FileFormatResult>> futures;
    futures.reserve(files.size());

    for (size_t i = 0; i < files.size(); i++) {
        futures.push_back(pool.submit_task([&, i]() {
            if (!reportCompletions)
                return formatInput(i);
            if (isVerbose)
                reportProgress(i, false);
            ScopeGuard finished([&] { reportProgress(i, true); });
            return formatInput(i);
        }));
    }

    if (reportCompletions) {
        // Consume worker events instead of waiting on an earlier, possibly stalled file.
        for (size_t remaining = files.size(); remaining > 0;) {
            std::unique_lock lock(progressMutex);
            progressReady.wait(lock, [&] { return !progress.empty(); });
            auto event = progress.front();
            progress.pop_front();
            lock.unlock();

            if (event.finished) {
                auto result = futures[event.index].get();
                auto status = processResult(result);
                writeStats(result, status);
                if (isVerbose)
                    finishVerbose(result);
                remaining--;
            }
            else {
                OS::printE(fmt::format("formatting {} ...\n", files[event.index]));
            }
        }
    }
    else {
        for (auto& future : futures) {
            auto result = future.get();
            processResult(result);
        }
    }

    OS::printE(fmt::format("{} {} files", isDryRun ? "would format" : "formatted", formattedCount));
    OS::printE(
        fmt::format(
            ", {} unchanged, {} excluded, {} failed", unchangedCount, excludedCount, failedCount
        )
    );
    if (skippedCount > 0)
        OS::printE(fmt::format(", {} skipped (validation failures)", skippedCount));
    if (errorCount > 0)
        OS::printE(fmt::format(", {} errors", errorCount));
    if (cstMismatchCount > 0)
        OS::printE(fmt::format(" ({} CST mismatch)", cstMismatchCount));
    if (idempotentFailCount > 0)
        OS::printE(fmt::format(" ({} not idempotent)", idempotentFailCount));
    OS::printE("\n");

    bool reportFailed = closeStats();
    return errorCount > 0 || checkFailed || reportFailed ? 1 : 0;
}
