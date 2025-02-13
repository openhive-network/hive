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

#include <algorithm>
#include <cstring>

namespace
{

// Assuming #embed supported only by clang
#ifdef __clang__

const uint8_t word_list_zipped[] = {
#embed "words.zip"
};

#else
hive::words::const_char_ptr word_list_b64 =
#include <hive/protocol/words.b64>
;
#endif // __clang__

class word_list_t
  {
  public:
    word_list_t()
      {
      initialize();
      }

    ~word_list_t()
      {
      delete [] list;
      }

    const hive::words::const_char_ptr* get_list()
      {
      return list;
      }

  private:
    void initialize()
      {
#ifdef __clang__
      const std::string word_list_zip((const char*)word_list_zipped, sizeof(word_list_zipped));
#else
      const std::string word_list_zip = fc::base64_decode(word_list_b64);
#endif //__clang__
      raw_list = fc::zip_decompress(word_list_zip);
      list = new char*[hive::words::word_list_size];

      for (uint32_t pos = 0, i = 0; i != hive::words::word_list_size; ++i)
        {
        list[i] = raw_list.data() + pos;
        pos = raw_list.find('\n', pos);
        if (pos == std::string::npos)
          break;
        raw_list[pos] = '\0';
        ++pos;
        }
      }

  private:
    char**      list = nullptr;
    std::string raw_list;
  };

} // namespace

namespace hive { namespace words {

const const_char_ptr* get_word_list()
  {
  static word_list_t  word_list;
  return word_list.get_list();
  }

} } // hive::words
