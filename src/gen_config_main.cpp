//------------------------------------------------------------------------------
// gen_config_main.cpp
// Standalone tool to generate JSON schema from FormatConfig.h
//
// SPDX-FileCopyrightText: Hudson River Trading
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------

#include "format/FormatConfig.h"
#include <iostream>
#include <rfl/DefaultIfMissing.hpp>
#include <rfl/json.hpp>

int main() {
    const std::string schema = rfl::json::to_schema<format::Config, rfl::DefaultIfMissing>(
        rfl::json::pretty | YYJSON_WRITE_PRETTY_TWO_SPACES
    );
    std::cout << schema << '\n';
    return 0;
}
