// clang-format off
#include <print>
#include "hook.hpp"
// clang-format on

// external libs
//  #include "toml.hpp"

auto main( int argc, char** argv ) -> int {
    if ( argc > 1 && std::strcmp( argv[1], "install" ) == 0 ) {
        if ( !hook::install( argc, argv ) ) {
            std::println( "[-] installation failed..." );
            return 1;
        }
    }

    if ( argc < 3 ) { // bin - remote - url
        std::println( "[-] invalid argument count!" );
        return 1;
    }

    return hook::run( argv[2] ) ? 0 : 1;
}
