#include <filesystem>
#include <print>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <cctype>

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

std::string extract_owner(std::string_view url) {
    std::string str{};

    if (url.ends_with('/')) {
        url.remove_suffix(1);
    }

    std::string suffix = ".git";

    if (url.ends_with(suffix)) {
        url.remove_suffix(suffix.size());
    }


    auto last_pos = url.find_last_of("/:");
    if (last_pos == std::string::npos || last_pos == 0) {
        return {};
    }

    auto pos = url.find_last_of("/:", last_pos - 1);
    if (pos == std::string::npos) {
        return {};
    }

    str = url.substr(pos + 1, last_pos - pos - 1);

    std::transform(str.begin(), str.end(), str.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return str;
}

auto main( int argc, char** argv ) -> int {
    if (argc < 3) { //bin - remote - url
        std::println(stderr, "[-] invalid argument count!");
        //todo: print help?
        return 1;
    }

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
    
    auto owner = extract_owner(argv[2]);
    if (owner.empty()) {
        std::println(stderr, "[-] failed to get owner of this repository, blocking this push!");
        return 1;
    }
    std::println("[+] found repository owner: {}", owner);

    return 0;
}
