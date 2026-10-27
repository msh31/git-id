#pragma once
// clang-format off
#include <unordered_map>
#include <vector>
#include <filesystem>
// clang-format on

// dirty cheats
namespace fs = std::filesystem;

using rules_t = std::unordered_map<std::string, std::vector<std::string>>;

// struct config_t {}; //TODO!

namespace config {
    auto get_path( ) -> fs::path;
    auto trim( std::string str ) -> std::string;

    auto load_rules( const fs::path& p ) -> rules_t;
}; // namespace config
