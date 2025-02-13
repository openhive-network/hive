#pragma once

#include <fc/string.hpp>

namespace fc 
{

  /** Allows to perform low level ZLib compression (deflate process, i.e. being a part of work done by [g]zip tools)
  *   \param in input byte buffer to be compressed
  *   \return byte buffer holding compressed data.
  */
  string zlib_compress(const string& in);
  /** Allows to unpack contents of zip archive passed as input byte string.
   *  \param in input byte buffer to be opened as .zip archive
   *  \returns byte buffer holding inflated (uncompressed) data.
  */
  string zip_decompress(const string& in);

} // namespace fc
