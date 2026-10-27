#pragma once
#include <string>

namespace hook {
    auto install( int argc, char** argv ) -> bool;
    auto run( std::string_view path ) -> bool;
} // namespace hook
