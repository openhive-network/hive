#ifndef ZSTD_STATIC_LINKING_ONLY
# define ZSTD_STATIC_LINKING_ONLY
#endif
#include <zstd.h>

#include <thread>
#include <mutex>
#include <fc/log/logger.hpp>
#include <fc/exception/exception.hpp>
#include <hive/chain/block_compression_dictionaries.hpp>
#include <hive/chain/raw_compression_dictionaries.hpp>

namespace hive { namespace chain {

#ifdef HAS_COMPRESSION_DICTIONARIES

// we store our dictionaries in compressed form, this is the maximum size
// one will be when decompressed.  At the time of writing, we've decided
// to use 220K dictionaries
#define MAX_DICTIONARY_LENGTH (1 << 20)

std::mutex dictionaries_mutex;

struct decompressed_raw_dictionary_info
{
  std::unique_ptr<char[]> buffer;
  size_t size;
};

// maps dictionary_number to zstd dictionaries
typedef std::map<uint8_t, decompressed_raw_dictionary_info> decompressed_raw_dictionary_map_t;
decompressed_raw_dictionary_map_t decompressed_raw_dictionaries;

// maps a dictionary_number to the ready-to-use decompression dictionary
std::map<uint8_t, ZSTD_DDict*> decompression_dictionaries;

// maps a (dictionary_number, compression_level) pair to a ready-to-use compression dictionary
std::map<std::pair<uint8_t, int>, ZSTD_CDict*> compression_dictionaries;

// helper function, assumes the upper level function holds the mutex on our maps
const decompressed_raw_dictionary_info& get_decompressed_raw_dictionary(uint8_t dictionary_number)
{
  auto decompressed_dictionary_iter = decompressed_raw_dictionaries.find(dictionary_number);
  if (decompressed_dictionary_iter == decompressed_raw_dictionaries.end())
  {
    // we don't.  do we have the raw, compressed dictionary?
    auto raw_iter = raw_dictionaries.find(dictionary_number);
    if (raw_iter == raw_dictionaries.end())
      FC_THROW_EXCEPTION(fc::key_not_found_exception, "No dictionary ${dictionary_number} available", (dictionary_number));

    // if so, uncompress it
    std::unique_ptr<char[]> buffer(new char[MAX_DICTIONARY_LENGTH]);
    size_t uncompressed_dictionary_size = ZSTD_decompress(buffer.get(), MAX_DICTIONARY_LENGTH,
                                                          raw_iter->second.buffer, raw_iter->second.size);
    if (ZSTD_isError(uncompressed_dictionary_size))
      FC_THROW("Error decompressing dictionary ${dictionary_number} with zstd", (dictionary_number));

    // ilog("Decompressing dictionary number ${dictionary_number}, expanded ${compressed_size} to ${uncompressed_dictionary_size}", 
    //      (dictionary_number)("compressed_size", raw_iter->second.size)(uncompressed_dictionary_size));

    // copy into a memory buffer of exactly the right size
    std::unique_ptr<char[]> resized_buffer(new char[uncompressed_dictionary_size]);
    memcpy(resized_buffer.get(), buffer.get(), uncompressed_dictionary_size);

    // store off the uncompressed version
    bool insert_succeeded;
    std::tie(decompressed_dictionary_iter, insert_succeeded) = decompressed_raw_dictionaries.insert(std::make_pair(dictionary_number, 
                                                                                                                   decompressed_raw_dictionary_info{std::move(resized_buffer), uncompressed_dictionary_size}));
    if (!insert_succeeded)
      FC_THROW("Error storing decompressing dictionary ${dictionary_number}", (dictionary_number));
  }
  return decompressed_dictionary_iter->second;
}

std::optional<uint8_t> get_best_available_zstd_compression_dictionary_number_for_block(uint32_t block_number)
{
  uint8_t last_available_dictionary = raw_dictionaries.rbegin()->first;
  return std::min<uint8_t>(block_number / 1000000, last_available_dictionary);
}

std::optional<uint8_t> get_last_available_zstd_compression_dictionary_number()
{
  return raw_dictionaries.rbegin()->first;
}

ZSTD_DDict* get_zstd_decompression_dictionary(uint8_t dictionary_number)
{
  std::lock_guard<std::mutex> guard(dictionaries_mutex);

  // try to find the dictionary already fully loaded for decompression
  auto iter = decompression_dictionaries.find(dictionary_number);
  if (iter != decompression_dictionaries.end())
    return iter->second;

  // it's not there, see if we have, or can create, a decompressed raw dictionary
  const decompressed_raw_dictionary_info& decompressed_dictionary = get_decompressed_raw_dictionary(dictionary_number);

  // then create a usable decompression dictionary for it
  ZSTD_DDict* dictionary = ZSTD_createDDict_byReference(decompressed_dictionary.buffer.get(), decompressed_dictionary.size);
  decompression_dictionaries[dictionary_number] = dictionary;
  return dictionary;
}

ZSTD_CDict* get_zstd_compression_dictionary(uint8_t dictionary_number, int compression_level)
{
  std::lock_guard<std::mutex> guard(dictionaries_mutex);
  auto iter = compression_dictionaries.find(std::make_pair(dictionary_number, compression_level));
  if (iter != compression_dictionaries.end())
    return iter->second;

  // it's not there, see if we have, or can create, a decompressed raw dictionary
  const decompressed_raw_dictionary_info& decompressed_dictionary = get_decompressed_raw_dictionary(dictionary_number);

  ZSTD_CDict* dictionary = ZSTD_createCDict_byReference(decompressed_dictionary.buffer.get(), decompressed_dictionary.size, compression_level);
  compression_dictionaries[std::make_pair(dictionary_number, compression_level)] = dictionary;
  return dictionary;
}

#else // !defined(HAS_COMPRESSION_DICTIONARIES)
std::optional<uint8_t> get_best_available_zstd_compression_dictionary_number_for_block(uint32_t block_number)
{
  return std::optional<uint8_t>();
}

std::optional<uint8_t> get_last_available_zstd_compression_dictionary_number()
{
  return std::optional<uint8_t>();
}

ZSTD_DDict* get_zstd_decompression_dictionary(uint8_t dictionary_number)
{
  FC_THROW_EXCEPTION(fc::key_not_found_exception, "No dictionary ${dictionary_number} available -- hived was not built with compression dictionaries", (dictionary_number));
}

ZSTD_CDict* get_zstd_compression_dictionary(uint8_t dictionary_number, int compression_level)
{
  FC_THROW_EXCEPTION(fc::key_not_found_exception, "No dictionary ${dictionary_number} available -- hived was not built with compression dictionaries", (dictionary_number));
}
#endif // HAS_COMPRESSION_DICTIONARIES
 
} } // end namespace hive::chain
