#include "config.hpp"
#include <fstream>
#include <sstream>

auto config::get_path( ) -> fs::path {
    const char* home = nullptr;

#ifdef _WIN32
    home = std::getenv( "USERPROFILE" );
#else
    home = std::getenv( "HOME" );
#endif

    if ( home ) return home;
    return { };
};

// why cant we have trim in C++..?
auto config::trim( std::string str ) -> std::string {
    auto start = str.find_first_not_of( " \t\r\n" );
    if ( start == str.npos ) {
        return { };
    }

    auto end = str.find_last_not_of( " \t\r\n" );
    return str.substr( start, end - start + 1 );
}

auto config::load_rules( const fs::path& p ) -> rules_t {
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

        std::istringstream kss( key );
        std::string owner_key{ };
        std::vector<std::string> owners{ };
        while ( std::getline( kss, owner_key, '|' ) ) {
            auto trimmed = trim( owner_key );
            if ( trimmed.empty( ) ) {
                continue;
            }
            owners.push_back( trimmed );
        }

        std::istringstream ss( val );
        std::string email{ };
        while ( std::getline( ss, email, ',' ) ) {
            auto trimmed = trim( email );
            if ( trimmed.empty( ) ) {
                continue;
            }
            for ( const auto& owner : owners ) {
                rules[owner].push_back( trimmed );
            }
        }
    }

    return rules;
}
