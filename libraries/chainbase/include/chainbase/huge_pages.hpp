#pragma once

#include <boost/filesystem/path.hpp>

#include <cstddef>
#include <istream>
#include <optional>
#include <string>

namespace chainbase {

  /// Asks the kernel to back a mapping with huge pages, as madvise( MADV_HUGEPAGE ) does, returning 0 or an errno
  /// value. It takes effect only for files on tmpfs mounted with huge=advise or a stronger huge= option.
  int advise_huge_pages( void* address, std::size_t size );

  /// Whether path is on tmpfs, by statfs; false when that cannot be told.
  bool is_on_tmpfs( const boost::filesystem::path& path );

  /// The mount point of the tmpfs holding path, from the text of /proc/self/mountinfo, when that tmpfs is mounted
  /// without huge pages (its huge= option is absent or never); nothing for any other filesystem or a tmpfs that
  /// gives huge pages. path must be absolute with its symlinks resolved; the mount covering it is the one with the
  /// longest mount point, the later one in mountinfo when two share a mount point.
  std::optional< std::string > tmpfs_mount_without_huge_pages( std::istream& mountinfo, const boost::filesystem::path& path );

  /// Logs a warning naming the mount, once per process, when dir is on tmpfs mounted without huge pages.
  void warn_once_if_on_tmpfs_without_huge_pages( const boost::filesystem::path& dir );

} // namespace chainbase
