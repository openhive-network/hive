#pragma once

#include <fc/string.hpp>

namespace fc 
{

  string zlib_compress(const string& in);
  string gzip_decompress(const string& in);

} // namespace fc
