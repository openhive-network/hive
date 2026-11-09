/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
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
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hive { namespace words {

typedef const char* const_char_ptr;

/// Word list from its deflate compressed text, one word per line (the content of words.deflate).
class word_list
  {
  public:
    explicit word_list(const std::string& deflated_words);
    word_list(const word_list&) = delete;
    word_list& operator=(const word_list&) = delete;

    const const_char_ptr* get_list() const
      {
      return list.data();
      }

    uint32_t get_list_size() const
      {
      return list.size();
      }

  private:
    std::vector<const_char_ptr>  list;
    std::string                  raw_list;
  };

/// The word list built into the library.
const word_list& get_built_in_word_list();
const const_char_ptr* get_word_list();
uint32_t get_word_list_size();

} }
