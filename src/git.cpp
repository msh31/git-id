#include "git.hpp"
#include <algorithm>
#include <print>

// clang-format off
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#if defined( __APPLE__ )
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cerrno>
#include <mach-o/dyld.h>
#endif

#if defined (__FreeBSD__)
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <limits.h>
#include <cerrno>
#endif
// clang-format on

auto git::get_own_path( ) -> std::optional<std::filesystem::path> {
    std::filesystem::path p{ };

#ifdef _WIN32
    tpf += ".exe";
    char buffer[MAX_PATH];
    if ( GetModuleFileNameA( nullptr, buffer, sizeof( buffer ) ) == 0 ) {
        std::println( stderr, "[-] failed to get own path, abandoning install." );
        return std::nullopt;
    }
    sp = buffer;
#elif defined( __FreeBSD__ )
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PATHNAME;
    mib[3] = -1;

    char buffer[PATH_MAX];
    size_t b_sz = sizeof( buffer );
    if ( sysctl( mib, 4, buffer, &b_sz, nullptr, 0 ) == -1 ) {
        std::println( stderr, "[-] failed to get own path, abandoning install." );
        return std::nullopt;
    }
    sp = buffer;
#elif defined( __APPLE__ )
    // Source - https://stackoverflow.com/a/1024933
    // Posted by mark4o, modified by community. See post 'Timeline' for change history
    // Retrieved 2026-10-19, License - CC BY-SA 4.0

    char path[1024];
    uint32_t size = sizeof( path );
    if ( _NSGetExecutablePath( path, &size ) == 0 ) {
        p = path;
    } else {
        std::println( stderr, "[-] failed to get own path, abandoning install." );
        return std::nullopt;
    }
#else
    #error "UNSUPPORTED_PLATFORM"
#endif

    return p;
}

auto git::extract_owner( std::string_view url ) -> std::string {
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

auto git::run_git( const std::vector<std::string>& args ) -> std::optional<std::string> {
    std::string str{ };

#if defined( __APPLE__ ) || defined( __FreeBSD__ )
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
            std::println( stderr, "waitpid failed: {}", strerror( errno ) );
            return std::nullopt;
        }
        if ( !WIFEXITED( status ) ) {
            std::println( stderr, "waitpid failed, it was killed abruptly." );
            return std::nullopt;
        }
        if ( WEXITSTATUS( status ) != 0 ) {
            std::println( stderr, "waitpid failed: status: {}", WEXITSTATUS( status ) );
            return std::nullopt;
        }
    }

    if ( pid == 0 ) {
        if ( dup2( fds[1], STDOUT_FILENO ) == -1 ) {
            std::println( stderr, "dup2 failed: {}", strerror( errno ) );
            _exit( 1 );
        }

        // can fail but whatever
        close( fds[0] );
        close( fds[1] );

        std::vector<char*> argv;
        for ( auto& a : args ) {
            argv.push_back( const_cast<char*>( a.data( ) ) );
        }
        argv.push_back( nullptr );

        execvp( "git", argv.data( ) );
        _exit( 127 );
    }
#elifdef _WIN32
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof( si ) );
    si.cb = sizeof( si );
    ZeroMemory( &pi, sizeof( pi ) );

    SECURITY_ATTRIBUTES sa{ sizeof( sa ), nullptr, TRUE };
    HANDLE read_end{ }, write_end{ };
    CreatePipe( &read_end, &write_end, &sa, 0 );
    SetHandleInformation( read_end, HANDLE_FLAG_INHERIT, 0 );

    si.hStdOutput = write_end;
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdError = GetStdHandle( STD_ERROR_HANDLE );

    std::string argz{ };
    for ( auto& a : args ) {
        if ( !argz.empty( ) ) argz += ' ';
        argz += std::format( "{}{}{}", "\"", a, "\"" );
    }

    if ( !CreateProcessA( NULL, argz.data( ), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi ) ) {
        std::println( stderr, "[-] CreateProcess failed {}", GetLastError( ) );
        CloseHandle( write_end );
        CloseHandle( read_end );
        return std::nullopt;
    }
    CloseHandle( write_end );

    char buffer[4096];
    DWORD n = 0;
    while ( ReadFile( read_end, buffer, sizeof buffer, &n, nullptr ) && n > 0 ) {
        str.append( buffer, n );
    }
    CloseHandle( read_end );

    WaitForSingleObject( pi.hProcess, INFINITE );

    DWORD code = 0;
    GetExitCodeProcess( pi.hProcess, &code );
    if ( code != 0 ) {
        std::println( stderr, "[-] git failure, exit code: {}", code );
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
        return std::nullopt;
    }
    CloseHandle( pi.hProcess ) CloseHandle( pi.hThread );
#else
    #error "UNSUPPORTED_PLATFORM"
#endif
    return str;
}

auto git::list_outgoing_author_emails( const std::string& local, const std::string& remote )
    -> std::optional<std::string> {
    std::vector<std::string> args = { "git", "log", "--format=%ae%n%ce" };

    if ( remote.find_first_not_of( '0' ) == remote.npos ) {
        args.insert( args.end( ), { local, "--not", "--remotes" } );
    } else {
        args.push_back( remote + ".." + local );
    }

    return run_git( args );
}
