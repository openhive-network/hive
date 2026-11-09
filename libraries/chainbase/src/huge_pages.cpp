#include <chainbase/huge_pages.hpp>

#include <fc/log/logger.hpp>

#include <boost/filesystem/operations.hpp>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

#include <sys/mman.h>
#ifdef __linux__
#include <linux/magic.h>
#include <sys/vfs.h>
#endif

namespace chainbase {

  namespace
  {
    /// Undoes the octal escapes (\040 for a space and so on) of a mountinfo path.
    std::string unescape_mountinfo_path( const std::string& escaped )
    {
      std::string path;
      for( std::size_t i = 0; i < escaped.size(); ++i )
      {
        if( escaped[ i ] == '\\' && i + 3 < escaped.size() &&
            escaped[ i + 1 ] >= '0' && escaped[ i + 1 ] <= '3' &&
            escaped[ i + 2 ] >= '0' && escaped[ i + 2 ] <= '7' &&
            escaped[ i + 3 ] >= '0' && escaped[ i + 3 ] <= '7' )
        {
          path.push_back( static_cast< char >( ( escaped[ i + 1 ] - '0' ) * 64 + ( escaped[ i + 2 ] - '0' ) * 8 + ( escaped[ i + 3 ] - '0' ) ) );
          i += 3;
        }
        else
          path.push_back( escaped[ i ] );
      }
      return path;
    }

    bool is_under( const std::string& path, const std::string& mount_point )
    {
      if( mount_point == "/" )
        return true;
      return path.compare( 0, mount_point.size(), mount_point ) == 0 &&
             ( path.size() == mount_point.size() || path[ mount_point.size() ] == '/' );
    }

    /// The value of the huge= option in a comma separated option list, empty when it is absent.
    std::string huge_option( const std::string& options )
    {
      std::istringstream list( options );
      std::string option;
      while( std::getline( list, option, ',' ) )
        if( option.compare( 0, 5, "huge=" ) == 0 )
          return option.substr( 5 );
      return std::string();
    }

    std::atomic< bool > tmpfs_checked_once{ false };
  }

  int advise_huge_pages( void* address, std::size_t size )
  {
#ifdef MADV_HUGEPAGE
    return ::madvise( address, size, MADV_HUGEPAGE ) == 0 ? 0 : errno;
#else
    return EINVAL;
#endif
  }

  bool is_on_tmpfs( const boost::filesystem::path& path )
  {
#ifdef __linux__
    struct statfs info;
    return ::statfs( path.generic_string().c_str(), &info ) == 0 && info.f_type == TMPFS_MAGIC;
#else
    return false;
#endif
  }

  std::optional< std::string > tmpfs_mount_without_huge_pages( std::istream& mountinfo, const boost::filesystem::path& path )
  {
    const std::string target = path.generic_string();
    std::string best_mount_point;
    std::string best_fs_type;
    std::string best_options;
    bool found = false;

    std::string line;
    while( std::getline( mountinfo, line ) )
    {
      // mount_id parent_id major:minor root mount_point mount_options [optional_fields...] - fs_type source super_options
      std::istringstream fields( line );
      std::vector< std::string > tokens;
      for( std::string token; fields >> token; )
        tokens.push_back( token );
      if( tokens.size() < 5 )
        continue;
      std::size_t separator = 6;
      while( separator < tokens.size() && tokens[ separator ] != "-" )
        ++separator;
      if( separator + 1 >= tokens.size() )
        continue;

      const std::string mount_point = unescape_mountinfo_path( tokens[ 4 ] );
      if( !is_under( target, mount_point ) || ( found && mount_point.size() < best_mount_point.size() ) )
        continue;
      best_mount_point = mount_point;
      best_fs_type = tokens[ separator + 1 ];
      best_options = separator + 3 < tokens.size() ? tokens[ separator + 3 ] : std::string();
      found = true;
    }

    if( !found || best_fs_type != "tmpfs" )
      return std::nullopt;
    const std::string huge = huge_option( best_options );
    if( !huge.empty() && huge != "never" )
      return std::nullopt;
    return best_mount_point;
  }

  void warn_once_if_on_tmpfs_without_huge_pages( const boost::filesystem::path& dir )
  {
    if( tmpfs_checked_once.load() || !is_on_tmpfs( dir ) || tmpfs_checked_once.exchange( true ) )
      return;

    boost::system::error_code error;
    boost::filesystem::path resolved = boost::filesystem::canonical( dir, error );
    if( error )
      resolved = dir;

    std::ifstream mountinfo( "/proc/self/mountinfo" );
    if( !mountinfo )
    {
      wlog( "The state file directory ${dir} is on tmpfs, but /proc/self/mountinfo could not be read to tell whether "
            "the mount gives huge pages: ${error}", ( "dir", resolved.generic_string() )( "error", std::strerror( errno ) ) );
      return;
    }

    if( const auto mount_point = tmpfs_mount_without_huge_pages( mountinfo, resolved ) )
      wlog( "The state file directory ${dir} is on the tmpfs mounted at ${mount} without huge pages, so the state is "
            "mapped with 4 KiB pages; a mainnet replay was measured about 18% slower than on a tmpfs mounted with "
            "huge=within_size. Mount it with huge=within_size (e.g. mount -o remount,huge=within_size ${mount}) or "
            "huge=advise; see doc/run_hived_img.md",
            ( "dir", resolved.generic_string() )( "mount", *mount_point ) );
  }

} // namespace chainbase
