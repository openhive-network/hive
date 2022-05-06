#include <thread>
#include <mutex>
#include <fc/log/logger.hpp>
#include <fc/exception/exception.hpp>
#include <hive/chain/block_compression_dictionaries.hpp>

// we store our dictionaries in compressed form, this is the maximum size
// one will be when decompressed.  At the time of writing, we've decided
// to use 220K dictionaries
#define MAX_DICTIONARY_LENGTH (1 << 20)

std::mutex dictionaries_mutex;
struct raw_dictionary_info
{
  const void* buffer;
  unsigned size;
};
struct decompressed_raw_dictionary_info
{
  std::unique_ptr<char[]> buffer;
  size_t size;
};

// maps dictionary_number to raw data (zstd compressed zstd dictionaries)
std::map<uint8_t, raw_dictionary_info> raw_dictionaries;

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

    ilog("Decompressing dictionary number ${dictionary_number}, expanded ${compressed_size} to ${uncompressed_dictionary_size}", 
         (dictionary_number)("compressed_size", raw_iter->second.size)(uncompressed_dictionary_size));

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

void init_raw_dictionaries();
ZSTD_DDict* get_zstd_decompression_dictionary(uint8_t dictionary_number)
{
  init_raw_dictionaries();
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
  init_raw_dictionaries();
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
 
extern "C"
{
#define DICT(prefixed_num) \
  extern unsigned char __ ## prefixed_num ## M_dict_zst[]; \
  extern unsigned __ ## prefixed_num ## M_dict_zst_len;
  DICT(000);
  DICT(001);
  DICT(002);
  DICT(003);
  DICT(004);
  DICT(005);
  DICT(006);
  DICT(007);
  DICT(008);
  DICT(009);
  DICT(010);
  DICT(011);
  DICT(012);
  DICT(013);
  DICT(014);
  DICT(015);
  DICT(016);
  DICT(017);
  DICT(018);
  DICT(019);
  DICT(020);
  DICT(021);
  DICT(022);
  DICT(023);
  DICT(024);
  DICT(025);
  DICT(026);
  DICT(027);
  DICT(028);
  DICT(029);
  DICT(030);
  DICT(031);
  DICT(032);
  DICT(033);
  DICT(034);
  DICT(035);
  DICT(036);
  DICT(037);
  DICT(038);
  DICT(039);
  DICT(040);
  DICT(041);
  DICT(042);
  DICT(043);
  DICT(044);
  DICT(045);
  DICT(046);
  DICT(047);
  DICT(048);
  DICT(049);
  DICT(050);
  DICT(051);
  DICT(052);
  DICT(053);
  DICT(054);
  DICT(055);
  DICT(056);
  DICT(057);
  DICT(058);
  DICT(059);
  DICT(060);
  DICT(061);
  DICT(062);
#undef DICT
}

void init_raw_dictionaries()
{
  std::lock_guard<std::mutex> guard(dictionaries_mutex);
  bool initialized = false;
  if (initialized)
    return;
  initialized = true;
#define DICT(num, prefixed_num) \
  raw_dictionaries[num] = {(const void*)__ ## prefixed_num ## M_dict_zst, (unsigned)__ ## prefixed_num ## M_dict_zst_len};
  DICT(0, 000);
  DICT(1, 001);
  DICT(2, 002);
  DICT(3, 003);
  DICT(4, 004);
  DICT(5, 005);
  DICT(6, 006);
  DICT(7, 007);
  DICT(8, 008);
  DICT(9, 009);
  DICT(10, 010);
  DICT(11, 011);
  DICT(12, 012);
  DICT(13, 013);
  DICT(14, 014);
  DICT(15, 015);
  DICT(16, 016);
  DICT(17, 017);
  DICT(18, 018);
  DICT(19, 019);
  DICT(20, 020);
  DICT(21, 021);
  DICT(22, 022);
  DICT(23, 023);
  DICT(24, 024);
  DICT(25, 025);
  DICT(26, 026);
  DICT(27, 027);
  DICT(28, 028);
  DICT(29, 029);
  DICT(30, 030);
  DICT(31, 031);
  DICT(32, 032);
  DICT(33, 033);
  DICT(34, 034);
  DICT(35, 035);
  DICT(36, 036);
  DICT(37, 037);
  DICT(38, 038);
  DICT(39, 039);
  DICT(40, 040);
  DICT(41, 041);
  DICT(42, 042);
  DICT(43, 043);
  DICT(44, 044);
  DICT(45, 045);
  DICT(46, 046);
  DICT(47, 047);
  DICT(48, 048);
  DICT(49, 049);
  DICT(50, 050);
  DICT(51, 051);
  DICT(52, 052);
  DICT(53, 053);
  DICT(54, 054);
  DICT(55, 055);
  DICT(56, 056);
  DICT(57, 057);
  DICT(58, 058);
  DICT(59, 059);
  DICT(60, 060);
  DICT(61, 061);
  DICT(62, 062);
}

