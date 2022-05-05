#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

#define DICTIONARY_NUMBER_FROM_BLOCK_NUMBER(x) (x / 1000000)

ZSTD_CDict* get_zstd_compression_dictionary(uint8_t dictionary_number, int compression_level);
ZSTD_DDict* get_zstd_decompression_dictionary(uint8_t dictionary_number);
