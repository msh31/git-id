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

fs::path config_path( ) {
    const char* home = nullptr;

#ifdef _WIN32
    home = std::getenv( "USERPROFILE" );
#else
    home = std::getenv( "HOME" );
#endif

    if ( home ) return home;
    return { };
}

std::string extract_owner( std::string_view url ) {
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
std::string trim( std::string str ) {
    auto start = str.find_first_not_of( " \t\r\n" );
    if ( start == str.npos ) {
        return { };
    }

    auto end = str.find_last_not_of( " \t\r\n" );
    return str.substr( start, end - start + 1 );
}

rules_t load_rules( const fs::path& p ) {
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

        std::istringstream ss( val );
        std::string email{ };
        while ( std::getline( ss, email, ',' ) ) {
            auto trimmed = trim( email );
            if ( trimmed.empty( ) ) {
                continue;
            }
            rules[key].push_back( trimmed );
        }
    }

    return rules;
}

auto list_outgoing_author_emails( const std::string& local, const std::string& remote ) -> std::string {
    std::string str{ };
#if defined( __linux__ ) || defined( __APPLE__ )
    int fds[2];
    pipe( fds );

    pid_t pid = fork( );
    pid_t w = 0;
    int status;

    if ( pid > 0 ) {
        close( fds[1] );

        char buffer[4096];
        ssize_t n;
        while ( ( n = read( fds[0], buffer, sizeof buffer ) ) != 0 ) {
            if ( n == -1 ) {
                if ( errno == EINTR ) continue;
                break;
            }
            str.append( buffer, n );
        }

        close( fds[0] );
        w = waitpid( pid, &status, 0 );
        if ( w == -1 ) {
            std::println( "waitpid failed: {}", strerror( errno ) );
            return { };
        }
    }

    if ( pid == 0 ) {
        if ( dup2( fds[1], STDOUT_FILENO ) == -1 ) {
            std::println( "dup2 failed: {}", strerror( errno ) );
            _exit( 1 );
        }

        // can fail but whatever
        close( fds[0] );
        close( fds[1] );

        std::vector<std::string> args{ "git", "log", "--format=%ae", remote + ".." + local };
        std::vector<char*> argv;
        for ( auto& a : args ) {
            argv.push_back( a.data( ) );
        }
        argv.push_back( nullptr );

        execvp( "git", argv.data( ) );
        _exit( 127 );
    }
#endif

#ifdef _WIN32
    // todo
    // https://learn.microsoft.com/en-us/windows/win32/procthread/creating-processes
#endif

    return str;
}

auto main( int argc, char** argv ) -> int {
    auto log_and_exit = []( std::string_view msg ) -> int {
        std::println( stderr, "{}", msg );
        return 1;
    };

    if ( argc < 3 ) { // bin - remote - url
        return log_and_exit( "[-] invalid argument count!" );
        // todo: print help?
    }

    auto c_path = config_path( );
    if ( c_path.empty( ) ) {
        return log_and_exit( "[-] failed to get environment variable for the config, blocking this push!" );
    }

    auto c_file = c_path / ".git-identities";
    if ( !fs::is_regular_file( c_file ) ) {
        return log_and_exit( "[-] config file not found, blocking this push!" );
    }

    auto rules = load_rules( c_file );
    if ( rules.empty( ) ) {
        return log_and_exit( "[-] failed to load rules, blocking this push!" ); // to be re-evaluated
    }

    auto owner = extract_owner( argv[2] );
    if ( owner.empty( ) ) {
        return log_and_exit( "[-] failed to get owner of this repository, blocking this push!" );
    }
    std::println( "[+] found repository owner: {}", owner );

    if ( rules.find( owner ) == rules.end( ) ) {
        return log_and_exit( "[-] owner was not found in the rules, blocking this push!" );
    }

    std::string line{ };
    std::string local_ref, local_oid, remote_ref, remote_oid;
    while ( std::getline( std::cin, line ) ) {
        std::istringstream iss( line );
        iss >> local_ref >> local_oid >> remote_ref >> remote_oid;
        if ( !iss ) {
            return log_and_exit( "[-] failed to split refs, blocking this push!" );
        }
        std::println( "[+] found refs: {} {} {} {}", local_ref, local_oid, remote_ref, remote_oid );

        auto out_mails = list_outgoing_author_emails( local_oid, remote_oid );
        if ( out_mails.empty( ) ) {
            return log_and_exit( "[-] failed to list outgoing author emails, blocking this push" ); // laziness
        }
        std::println( "[+] outgoing author mails: {}", out_mails );

        std::istringstream ss( out_mails );
        std::string email{ };
        while ( std::getline( ss, email ) ) {
            if ( email.empty( ) ) continue;

            std::transform(
                email.begin( ), email.end( ), email.begin( ), []( unsigned char c ) { return std::tolower( c ); } );

            const auto& allowed = rules.at( owner );
            if ( std::find( allowed.begin( ), allowed.end( ), email ) == allowed.end( ) ) {
                auto str = std::format( "[-] failed to find {}, blocking this push!", email );
                return log_and_exit( str );
            }
        }
    }

    return 0;
}
