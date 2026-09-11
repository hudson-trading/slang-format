// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT

#include "format/FormatConfig.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "slang/util/OS.h"

namespace fs = std::filesystem;

TEST_CASE("formatter config loading and discovery are shared") {
    auto root = fs::temp_directory_path() /
                ("slang_format_config_test_" + std::to_string(slang::OS::getpid()));
    fs::remove_all(root);
    fs::create_directories(root / ".slang");
    fs::create_directories(root / "src" / "nested");

    auto configPath = root / ".slang" / "format.json";
    std::ofstream(configPath) << R"({"columnLimit": 72})";

    auto found = format::findConfigFile(root / "src" / "nested");
    REQUIRE(found);
    CHECK(fs::equivalent(*found, configPath));

    std::string error = "stale error";
    auto config = format::loadConfigFile(*found, error);
    REQUIRE(config);
    CHECK(config->columnLimit.value() == 72);
    CHECK(error.empty());

    fs::remove_all(root);
}

TEST_CASE("formatter config loading reports parse failures") {
    auto path = fs::temp_directory_path() /
                ("slang_format_config_invalid_" + std::to_string(slang::OS::getpid()) + ".json");
    std::ofstream(path) << "{";

    std::string error;
    CHECK_FALSE(format::loadConfigFile(path, error));
    CHECK(error.find("failed to parse config file") != std::string::npos);

    fs::remove(path);
}

TEST_CASE("unknown config keys report actionable errors without parser advice") {
    std::string error;
    CHECK_FALSE(format::parseConfig(R"({"unknown": true})", error));
    CHECK(error == "Unknown configuration key 'unknown'.");

    CHECK_FALSE(
        format::parseConfig(
            R"({"excludeDirs": [], "dirs": [], "alignment": {"paddingLmit": 4, "groupLines": 2}, "indentWidth": "bad"})",
            error
        )
    );
    for (auto key : {"excludeDirs", "dirs", "paddingLmit", "groupLines"})
        CHECK(
            error.find("Unknown configuration key '" + std::string(key) + "'.") != std::string::npos
        );
    CHECK(error.find("field 'alignment'") != std::string::npos);
    CHECK(error.find("field 'indentWidth'") != std::string::npos);
    CHECK(error.find("rfl::") == std::string::npos);
    CHECK(error.find("not used") == std::string::npos);

    CHECK_FALSE(format::parseConfig(R"({"it's unknown": 1})", error));
    CHECK(error == "Unknown configuration key 'it's unknown'.");
    CHECK(format::parseConfig("{}", error));
    CHECK(error.empty());
}
