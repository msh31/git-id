#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace git {
    auto get_own_path( ) -> std::optional<std::filesystem::path>; // a bit odd to have here since its not git specific

    auto extract_owner( std::string_view url ) -> std::string;
    auto run_git( const std::vector<std::string>& args ) -> std::optional<std::string>;
    auto list_outgoing_author_emails( const std::string& local, const std::string& remote )
        -> std::optional<std::string>;
} // namespace git
