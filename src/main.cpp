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
    while ( std::getline( std::cin, line ) ) {
        std::istringstream iss( line );
        std::string local_ref, local_oid, remote_ref, remote_oid;
        iss >> local_ref >> local_oid >> remote_ref >> remote_oid;
        if ( !iss ) {
            return log_and_exit( "[-] failed to split refs, blocking this push!" );
        }
        std::println( "[+] found refs: {} {} {} {}", local_ref, local_oid, remote_ref, remote_oid );
    }

    return 0;
}
