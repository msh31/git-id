#pragma once
#include <filesystem>

namespace fs = std::filesystem;

// globalz
inline fs::path g_config_path( ) {
#ifdef _WIN32
    return std::getenv( "USERPROFILE" );
#else
    return std::getenv( "HOME" );
#endif
}
inline fs::path m_config_file = g_config_path( ) / "config.txt";
