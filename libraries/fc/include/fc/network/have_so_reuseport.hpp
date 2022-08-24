#pragma once

namespace fc { namespace detail {
#if defined( __APPLE__ ) || defined( __linux__ )
  static bool have_so_reuseport = true;
#endif
}} // namespace fc::detail
