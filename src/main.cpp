#include <filesystem>
#include <print>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <cctype>
#include <unordered_map>

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

std::unordered_map<std::string, std::string> load_rules() {
    std::unordered_map<std::string, std::string> res{};

    //TODO

    return res;
}

auto main( int argc, char** argv ) -> int {
    auto log_and_exit = [](std::string_view msg) -> int {
        std::println(stderr, "{}", msg);
        return 1;
    };

    if (argc < 3) { //bin - remote - url
        return log_and_exit("[-] invalid argument count!");
        //todo: print help?
    }

    auto c_path = config_path();
    if (c_path.empty()) {
        return log_and_exit("[-] failed to get environment variable for the config, blocking this push!");
    }

    auto c_file = c_path / ".git-identities";
    if (!fs::is_regular_file(c_file)) {
        return log_and_exit("[-] config file not found, blocking this push!");
    }

    auto rules = load_rules();
    if (rules.empty()) {
        return log_and_exit("[-] failed to load rules, blocking this push!"); //to be re-evaluated
    }
    
    auto owner = extract_owner(argv[2]);
    if (owner.empty()) {
        return log_and_exit("[-] failed to get owner of this repository, blocking this push!");
    }

    std::println("[+] found repository owner: {}", owner);

    return 0;
}
