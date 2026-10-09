// clang-format off
#include <filesystem>
#include <print>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <optional>

// platformzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#if defined( __linux__ ) || defined( __APPLE__ )
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cerrno>
#endif
// clang-format on

namespace fs = std::filesystem;

using rules_t = std::unordered_map<std::string, std::vector<std::string>>;

auto config_path( ) -> fs::path {
    const char* home = nullptr;

#ifdef _WIN32
    home = std::getenv( "USERPROFILE" );
#else
    home = std::getenv( "HOME" );
#endif

    if ( home ) return home;
    return { };
}

auto extract_owner( std::string_view url ) -> std::string {
    std::string str{ };

    if ( url.ends_with( '/' ) ) {
        url.remove_suffix( 1 );
    }

    std::string suffix = ".git";

    if ( url.ends_with( suffix ) ) {
        url.remove_suffix( suffix.size( ) );
    }

    auto last_pos = url.find_last_of( "/:" );
    if ( last_pos == std::string::npos || last_pos == 0 ) {
        return { };
    }

    auto pos = url.find_last_of( "/:", last_pos - 1 );
    if ( pos == std::string::npos ) {
        return { };
    }

    str = url.substr( pos + 1, last_pos - pos - 1 );

    std::transform( str.begin( ), str.end( ), str.begin( ), []( unsigned char c ) { return std::tolower( c ); } );
    return str;
}

// why cant we have trim in C++..?
auto trim( std::string str ) -> std::string {
    auto start = str.find_first_not_of( " \t\r\n" );
    if ( start == str.npos ) {
        return { };
    }

    auto end = str.find_last_not_of( " \t\r\n" );
    return str.substr( start, end - start + 1 );
}

auto load_rules( const fs::path& p ) -> rules_t {
    rules_t rules{ };

    std::ifstream in( p );
    if ( !in.is_open( ) ) {
        return { };
    }

    std::string line{ };
    while ( std::getline( in, line ) ) {
        auto pos = line.find( ':' );

        std::transform(
            line.begin( ), line.end( ), line.begin( ), []( unsigned char c ) { return std::tolower( c ); } );

        std::string key{ };
        std::string val{ };

        if ( pos != line.npos ) {
            key = trim( line.substr( 0, pos ) );
            val = line.substr( pos + 1 );
        }

        if ( key.empty( ) ) {
            continue;
        }

        std::istringstream kss(key);
        std::string owner_key{ };
        std::vector<std::string> owners{};
        while (std::getline(kss, owner_key, '|')) {
            auto trimmed = trim(owner_key);
            if (trimmed.empty()) {
                continue;
            }
            owners.push_back(trimmed);
        }

        std::istringstream ss( val );
        std::string email{ };
        while ( std::getline( ss, email, ',' ) ) {
            auto trimmed = trim( email );
            if ( trimmed.empty( ) ) {
                continue;
            }
            for (const auto& owner : owners) {
                rules[owner].push_back(trimmed);
            }
        }
    }

    return rules;
}

auto run_git(const std::vector<std::string>& args) -> std::optional<std::string> {
    std::string str{ };

#if defined( __linux__ ) || defined( __APPLE__ )
    int fds[2];
    pipe(fds);

    pid_t pid = fork();
    pid_t w = 0;
    int status;

    if (pid > 0) {
        close(fds[1]);

        char buffer[4096];
        ssize_t n;
        while ((n = read(fds[0], buffer, sizeof buffer)) != 0) {
            if (n == -1) {
                if (errno == EINTR) continue;
                break;
            }
            str.append(buffer, n);
        }

        close(fds[0]);
        w = waitpid(pid, &status, 0);
        if (w == -1) {
            std::println(stderr, "waitpid failed: {}", strerror(errno));
            return std::nullopt;
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::println(stderr, "waitpid failed: status: {}", WEXITSTATUS(status));
            return std::nullopt;
        }
    }

    if (pid == 0) {
        if (dup2(fds[1], STDOUT_FILENO) == -1) {
            std::println(stderr, "dup2 failed: {}", strerror(errno));
            _exit(1);
        }

        // can fail but whatever
        close(fds[0]);
        close(fds[1]);

        std::vector<char*> argv;
        for (auto& a : args) {
            argv.push_back(a.data());
        }
        argv.push_back(nullptr);

        execvp("git", argv.data());
        _exit(127);
    }
#endif

#ifdef _WIN32
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE read_end{ }, write_end{ };
    CreatePipe(&read_end, &write_end, &sa, 0);
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);

    si.hStdOutput = write_end;
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    std::string argz{};
    for (auto& a : args) {
        if (!argz.empty()) argz += ' ';
        argz += a;
    }

    if (!CreateProcessA(NULL, argz.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        std::println(stderr, "[-] CreateProcess failed {}", GetLastError());
        CloseHandle(write_end);
        CloseHandle(read_end);
        return std::nullopt;
    }
    CloseHandle(write_end);

    char buffer[4096];
    DWORD n = 0;
    while (ReadFile(read_end, buffer, sizeof buffer, &n, nullptr) && n > 0) {
        str.append(buffer, n);
    }
    CloseHandle(read_end);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    if (code != 0) {
        std::println(stderr, "[-] git failure, exit code: {}", code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return std::nullopt;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#endif

    return str;
}

auto list_outgoing_author_emails( const std::string& local, const std::string& remote ) -> std::optional<std::string> {

    std::vector<std::string> args = { "git", "log", "--format=%ae%n%ce" };

    if (remote.find_first_not_of('0') == remote.npos) {
        args.insert(args.end(), { local, "--not", "--remotes" });
    }
    else {
        args.push_back(remote + ".." + local);
    }

    return run_git(args);
}

auto main( int argc, char** argv ) -> int {
    auto log_and_exit = []( std::string_view msg ) -> int {
        std::println( stderr, "{}, blocking this push! You can bypass it by using 'git push --no-verify'", msg );
        return 1;
    };

    if (argc > 1 && std::strcmp(argv[1], "install") == 0) {
        bool is_global = argc > 2 && std::strcmp(argv[2], "--global") == 0;

        auto git_val = run_git({ "git", "rev-parse", "--git-path", "hooks" });
        if (!is_global && !git_val) {
            std::println(stderr, "[-] failed to find .git/hooks folder, are you sure this is a git repository?");
            return 1;
        }

        std::string hooks_dir = git_val.value_or("");
        hooks_dir.erase(hooks_dir.find_last_not_of("\r\n") + 1);

        fs::path tpf = "pre-push";
        fs::path sp{};
#ifdef _WIN32
        tpf += ".exe";
        char buffer[MAX_PATH] ;
        if (GetModuleFileNameA(nullptr, buffer, sizeof(buffer)) == 0) {
            std::println(stderr, "[-] failed to get own path, abandoning install.");
            return 1;
        }
        sp = buffer;
#elif defined (__linux__) || defined(__APPLE__)
        //todo
#endif

        fs::path tp{};
        if (is_global) {
            auto ltp = config_path() / ".git-id" / "hooks"; //unlikely that home or userprofile isn't set if so, well too bad
            tp = ltp / tpf;
            std::error_code ec;
            fs::create_directories(ltp, ec);
            if (ec) {
                std::println(stderr, "[-] failed to create global config directory: {}", ec.message());
                return 1;
            }
        }
        else {
            tp = ".git/hooks" / tpf;
        }

        if (fs::exists(tp)) {
            std::println(stderr, "[-] a pre-existing hook was found! abandoning install.");
            return 1;
        }

        std::error_code ec;
        fs::copy_file(sp, tp, ec);
        if (ec) {
            std::println(stderr, "[-] failed to copy file for install purposes because: {}", ec.message());
            return 1;
        }
        if (is_global) {
            auto rg_res = run_git({ "git", "config", "--global", "core.hooksPath", tp.parent_path().string() });
            if (rg_res == std::nullopt) {
                std::println(stderr, "[-] failed to install globally!");
                return 1;
            }
        }
        std::println(stdout, "[+] succesfully installed git-id here: {}", tp.string());
        return 0;
    }

    if ( argc < 3 ) { // bin - remote - url
        return log_and_exit( "[-] invalid argument count!" );
        // todo: print help?
    }

    auto c_path = config_path( );
    if ( c_path.empty( ) ) {
        return log_and_exit( "[-] failed to get environment variable for the config" );
    }

    auto c_file = c_path / ".git-identities";
    if ( !fs::is_regular_file( c_file ) ) {
        return log_and_exit( "[-] config file not found" );
    }

    auto rules = load_rules( c_file );
    if ( rules.empty( ) ) {
        return log_and_exit( "[-] failed to load rules" ); // to be re-evaluated
    }

    auto owner = extract_owner( argv[2] );
    if ( owner.empty( ) ) {
        return log_and_exit( "[-] failed to get owner of this repository" );
    }
    std::println( "[+] found repository owner: {}", owner );

    if ( rules.find( owner ) == rules.end( ) ) {
        return log_and_exit( "[-] owner was not found in the rules" );
    }

    std::string line{ };
    std::string local_ref, local_oid, remote_ref, remote_oid;
    while ( std::getline( std::cin, line ) ) {
        std::istringstream iss( line );
        iss >> local_ref >> local_oid >> remote_ref >> remote_oid;
        if ( !iss ) {
            return log_and_exit( "[-] failed to split refs" );
        }

        auto l_start = local_oid.find_first_not_of( '0' );
        if ( l_start == local_oid.npos ) { // deleting a remote branch requires no commits
            continue;
        }

        std::println( "[+] found refs: {} {} {} {}", local_ref, local_oid, remote_ref, remote_oid );

        auto out_mails = list_outgoing_author_emails( local_oid, remote_oid );
        if ( !out_mails.has_value( ) ) {
            return log_and_exit( "[-] failed to execute git commands, blocking this push" );
        }
        std::println( "[+] outgoing author mails: {}", out_mails.value() );

        std::istringstream ss( out_mails.value() );
        std::string email{ };
        while ( std::getline( ss, email ) ) {
            if ( email.empty( ) ) continue;

            std::transform(
                email.begin( ), email.end( ), email.begin( ), []( unsigned char c ) { return std::tolower( c ); } );

            const auto& allowed = rules.at( owner );
            if ( std::find( allowed.begin( ), allowed.end( ), email ) == allowed.end( ) ) {
                auto str = std::format( "[-] failed to find {}", email );
                return log_and_exit( str );
            }
        }
    }

    return 0;
}
