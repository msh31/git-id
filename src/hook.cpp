#include "hook.hpp"
#include "config.hpp"
#include "git.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <print>
#include <sstream>
#include <string>

namespace {
    auto log_and_block = []( std::string_view msg ) -> bool {
        std::println( stderr, "{}, blocking this push! You can bypass it by using 'git push --no-verify'", msg );
        return false;
    };
} // namespace

auto hook::install( int argc, char** argv ) -> bool {
    bool is_global = argc > 2 && std::strcmp( argv[2], "--global" ) == 0;

    auto git_val = git::run_git( { "git", "rev-parse", "--git-path", "hooks" } );
    if ( !is_global && !git_val ) {
        std::println( stderr, "[-] failed to find .git/hooks folder, are you sure this is a git repository?" );
        return false;
    }

    std::string hooks_dir = git_val.value_or( "" );
    hooks_dir.erase( hooks_dir.find_last_not_of( "\r\n" ) + 1 );

    fs::path tpf = "pre-push";
    // fs::path sp{ };

    fs::path tp{ };
    if ( is_global ) {
        auto ltp = config::get_path( ) / ".git-id" /
                   "hooks"; // unlikely that home or userprofile isn't set if so, well too bad
        tp = ltp / tpf;
        std::error_code ec;
        fs::create_directories( ltp, ec );
        if ( ec ) {
            std::println( stderr, "[-] failed to create global config directory: {}", ec.message( ) );
            return false;
        }
    } else {
        tp = ".git/hooks" / tpf;
    }

    if ( fs::exists( tp ) ) {
        std::println( stderr, "[-] a pre-existing hook was found! abandoning install." );
        return false;
    }

    auto sp = git::get_own_path( );
    if ( !sp.has_value( ) ) {
        std::println( stderr, "[-] failed to get own path! abandoning install." );
        return false;
    }

    std::error_code ec;
    fs::copy_file( sp.value( ), tp, ec );
    if ( ec ) {
        std::println( stderr, "[-] failed to copy file for install purposes because: {}", ec.message( ) );
        return false;
    }
    if ( is_global ) {
        auto rg_res = git::run_git( { "git", "config", "--global", "core.hooksPath", tp.parent_path( ).string( ) } );
        if ( rg_res == std::nullopt ) {
            std::println( stderr, "[-] failed to install globally!" );
            return false;
        }
    }
    std::println( stdout, "[+] succesfully installed git-id here: {}", tp.string( ) );
    return true;
}

auto hook::run( std::string_view path ) -> bool {
    auto c_path = config::get_path( );
    if ( c_path.empty( ) ) {
        return log_and_block( "[-] failed to get environment variable for the config" );
    }

    auto c_file = c_path / ".git-identities";
    if ( !fs::is_regular_file( c_file ) ) {
        return log_and_block( "[-] config file not found" );
    }

    auto rules = config::load_rules( c_file );
    if ( rules.empty( ) ) {
        return log_and_block( "[-] failed to load rules" ); // to be re-evaluated
    }

    auto owner = git::extract_owner( path );
    if ( owner.empty( ) ) {
        return log_and_block( "[-] failed to get owner of this repository" );
    }
    std::println( "[+] found repository owner: {}", owner );

    if ( rules.find( owner ) == rules.end( ) ) {
        return log_and_block( "[-] owner was not found in the rules" );
    }

    std::string line{ };
    std::string local_ref, local_oid, remote_ref, remote_oid;
    while ( std::getline( std::cin, line ) ) {
        std::istringstream iss( line );
        iss >> local_ref >> local_oid >> remote_ref >> remote_oid;
        if ( !iss ) {
            return log_and_block( "[-] failed to split refs" );
        }

        auto l_start = local_oid.find_first_not_of( '0' );
        if ( l_start == local_oid.npos ) { // deleting a remote branch requires no commits
            continue;
        }

        std::println( "[+] found refs: {} {} {} {}", local_ref, local_oid, remote_ref, remote_oid );

        auto out_mails = git::list_outgoing_author_emails( local_oid, remote_oid );
        if ( !out_mails.has_value( ) ) {
            return log_and_block( "[-] failed to execute git commands, blocking this push" );
        }
        std::println( "[+] outgoing author mails: {}", out_mails.value( ) );

        std::istringstream ss( out_mails.value( ) );
        std::string email{ };
        while ( std::getline( ss, email ) ) {
            if ( email.empty( ) ) continue;

            std::transform(
                email.begin( ), email.end( ), email.begin( ), []( unsigned char c ) { return std::tolower( c ); } );

            const auto& allowed = rules.at( owner );
            if ( std::find( allowed.begin( ), allowed.end( ), email ) == allowed.end( ) ) {
                auto str = std::format( "[-] failed to find {}", email );
                return log_and_block( str );
            }
        }
    }
    return true;
}
