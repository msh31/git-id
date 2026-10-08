#include <filesystem>
#include <print>
#include <cstdlib>
#include <string>

namespace fs = std::filesystem;

fs::path config_path() {
    const char* home = nullptr;

#ifdef _WIN32
    home = std::getenv("USERPROFILE");
#else
    home = std::getenv("HOME");
#endif

    if (home) return home;
    return {};
}

auto main( ) -> int {
    auto c_path = config_path();
    if (c_path.empty()) {
        std::println(stderr, "[-] failed to get environment variable for the config, blocking this push!");
        return 1;
    }

    auto c_file = c_path / ".git-identities";
    if (!fs::is_regular_file(c_file)) {
        std::println(stderr, "[-] config file not found, blocking this push!");
        return 1;
    }

    return 0;
}
