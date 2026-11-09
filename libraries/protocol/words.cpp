/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <hive/protocol/words.hpp>

#include <fc/crypto/base64.hpp>
#include <fc/compress/zlib.hpp>

#include <cstring>
#include <string>
#include <vector>

namespace
{

// Assuming #embed supported only by clang
#ifdef C23_EMBED_SUPPORTED
extern "C" const uint8_t word_list_zipped[];
extern "C" const uint32_t word_list_zipped_size;
#else
hive::words::const_char_ptr word_list_b64 =
#include <hive/protocol/words.b64>
;
static_assert(sizeof(word_list_b64) != 0);
#endif // C23_EMBED_SUPPORTED

std::string built_in_word_list_zip()
  {
#ifdef C23_EMBED_SUPPORTED
  return std::string((const char*)word_list_zipped, word_list_zipped_size);
#else
  return fc::base64_decode(word_list_b64);
#endif // C23_EMBED_SUPPORTED
  }

} // namespace

namespace hive { namespace words {

word_list::word_list(const std::string& deflated_words)
  : raw_list(fc::zlib_inflate(deflated_words))
  {
  for (size_t pos = 0; raw_list[pos] != '\0'; ++pos)
    {
    list.push_back(raw_list.data() + pos);
    pos = raw_list.find('\n', pos);
    if (pos == std::string::npos)
      break;
    raw_list[pos] = '\0';
    }
  }

const word_list& get_built_in_word_list()
  {
  static const word_list  built_in(built_in_word_list_zip());
  return built_in;
  }

const const_char_ptr* get_word_list()
  {
  return get_built_in_word_list().get_list();
  }

uint32_t get_word_list_size()
  {
  return get_built_in_word_list().get_list_size();
  }

} } // hive::words
