#include <cstdint>

extern "C"
{
  struct ZSTD_CDict_s;
  typedef struct ZSTD_CDict_s ZSTD_CDict;
  struct ZSTD_DDict_s;
  typedef struct ZSTD_DDict_s ZSTD_DDict;
}

namespace hive { namespace chain {
  fc::optional<uint8_t> get_best_available_zstd_compression_dictionary_number_for_block(uint32_t block_number);
  fc::optional<uint8_t> get_last_available_zstd_compression_dictionary_number();
  ZSTD_CDict* get_zstd_compression_dictionary(uint8_t dictionary_number, int compression_level);
  ZSTD_DDict* get_zstd_decompression_dictionary(uint8_t dictionary_number);
} }
