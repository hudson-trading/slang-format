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
#include <cstdio>
#include <filesystem>
#include <fmt/color.h>
#include <fmt/format.h>
#include <iostream>
#include <iterator>
#include <map>
#include <random>
#include <rfl/json.hpp>
#include <string>
#include <vector>

#include "slang/util/CommandLine.h"
#include "slang/util/OS.h"
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
    std::string error;
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
            "With no files or a single -, reads stdin and writes stdout.\n"
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
            "  --check, --verify Check formatting and validation without writing\n"
            "  --Werror         Fail on warnings and dry-run formatting changes\n"
            "  --strict         Fail on any validation failure without forcing output\n"
            "  --fail-on-incomplete-format  Alias for --strict\n"
            "  --failsafe_success=false    Alias for --strict\n"
            "  -f, --force       Force output even if validation fails\n"
            "  --config <path>   Path to config file. If omitted, searches for\n"
            "                    .slang/format.json walking up from the target\n"
            "                    path, then from the current directory\n"
            "  --assume-filename <path>  Use this stdin path for config and diagnostics\n"
            "  --stdin_name <path>      Alias for --assume-filename\n"
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

    auto outputResult = [&](const format::FormatResult& result, std::string_view input,
                            std::string_view path) {
        printDiagnostics(result, path);
        auto action = result.outputAction(force.value_or(false));
        if (action == format::FormatOutputAction::Abort)
            return 1;
        bool skipped = action == format::FormatOutputAction::KeepOriginal;
        bool changed = !skipped && result.formatted != input;
        if (noWrite && changed)
            OS::printE(fmt::format("{}: needs formatting\n", path));
        if (!noWrite)
            OS::print(skipped ? input : std::string_view(result.formatted));
        bool failed = (!result.isUsable() && (!skipped || strictValidation)) ||
                      (warningsAsErrors == true && !result.diagnostics.empty());
        return failed || (checkFormatting && changed) ? 1 : 0;
    };

    if (readStdin) {
        std::string input(std::istreambuf_iterator<char>(std::cin), {});
        auto resolved = resolveConfig(configSearchRoot(configTargets));
        return outputResult(
            format::format(stdinName, input, resolved.config, stage), input, stdinName
        );
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

    std::vector<ResolvedConfig> fileConfigs;
    fileConfigs.reserve(files.size());
    for (const auto& path : files)
        fileConfigs.push_back(resolveConfig(fs::path(path).parent_path()));
    auto formatInput = [&](size_t index) {
        if (!fileConfigs[index].error.empty()) {
            FileFormatResult result;
            result.path = files[index];
            result.error = fileConfigs[index].error;
            return result;
        }
        return formatFile(files[index], fileConfigs[index].config, stage);
    };

    // Single file without -i: output to stdout
    if (files.size() == 1 && inplace != true) {
        auto result = formatInput(0);
        if (!result.error.empty()) {
            OS::printE(fmt::format("error: {}\n", result.error));
            return 1;
        }
        return outputResult(result.result, result.input, result.path);
    }

    // Multiple files require -i (or --dry-run, which doesn't write)
    if (files.size() > 1 && inplace != true && !noWrite) {
        OS::printE("error: multiple files require -i, --dry-run, or --check\n");
        return 1;
    }

    // Format files with in-place editing
    const bool isVerbose = verbose.value_or(false);
    const uint32_t threads = isVerbose ? 1
                                       : numThreads.value_or(std::thread::hardware_concurrency());
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

    auto processResult = [&](FileFormatResult& result) {
        if (!result.error.empty()) {
            OS::printE(fmt::format("error: {}\n", result.error));
            errorCount++;
            failedCount++;
            return;
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
            return;
        }
        if (action == format::FormatOutputAction::KeepOriginal) {
            if (!result.result.generated)
                skippedCount++;
            else
                excludedCount++;
            return;
        }
        // Forced output with validation failures still returns an error status.
        if (!result.result.isUsable())
            errorCount++;

        if (result.result.formatted == result.input) {
            unchangedCount++;
            return;
        }
        if (isDryRun) {
            if (result.result.formatted != result.input) {
                OS::printE(fmt::format("{}: needs formatting\n", result.path));
                checkFailed = checkFailed || checkFormatting;
            }
            // Count successful formatting attempts without touching disk.
            formattedCount++;
            return;
        }

        std::string writeError;
        if (!writeFile(result.path, result.result.formatted, writeError)) {
            OS::printE(fmt::format("error: failed to write '{}': {}\n", result.path, writeError));
            errorCount++;
            if (result.result.isUsable())
                failedCount++;
        }
        else {
            formattedCount++;
        }
    };

    if (isVerbose) {
        // Verbose mode: format files sequentially with progress output
        for (size_t i = 0; i < files.size(); i++) {
            OS::printE(fmt::format("formatting {} ...\n", files[i]));
            auto result = formatInput(i);
            processResult(result);
        }
    }
    else {
        // Parallel mode: submit all files to thread pool
        std::vector<std::future<FileFormatResult>> futures;
        futures.reserve(files.size());

        for (size_t i = 0; i < files.size(); i++)
            futures.push_back(pool.submit_task([&, i]() { return formatInput(i); }));

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

    return errorCount > 0 || checkFailed ? 1 : 0;
}
