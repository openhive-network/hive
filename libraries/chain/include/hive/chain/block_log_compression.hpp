#pragma once

#include <hive/chain/detail/block_attributes.hpp>

extern "C"
{
  struct ZSTD_CCtx_s;
  typedef struct ZSTD_CCtx_s ZSTD_CCtx;
  struct ZSTD_DCtx_s;
  typedef struct ZSTD_DCtx_s ZSTD_DCtx;
}

namespace hive { namespace chain {

  /**
   * Separated to avoid including whole block_log header file.
   */
  class block_log_compression {
    public:
      using block_flags=detail::block_flags;
      using block_attributes_t=detail::block_attributes_t;

      static std::tuple<std::unique_ptr<char[]>, size_t> decompress_raw_block(
        std::tuple<std::unique_ptr<char[]>, size_t, block_attributes_t>&& raw_block_data_tuple);

      static std::tuple<std::unique_ptr<char[]>, size_t> decompress_raw_block(
        const char* raw_block_data, size_t raw_block_size, block_attributes_t attributes);

      static std::tuple<std::unique_ptr<char[]>, size_t> compress_block_zstd(
        const char* uncompressed_block_data, size_t uncompressed_block_size, 
        std::optional<uint8_t> dictionary_number, 
        fc::optional<int> compression_level = fc::optional<int>(), 
        fc::optional<ZSTD_CCtx*> compression_context = fc::optional<ZSTD_CCtx*>());

      static std::tuple<std::unique_ptr<char[]>, size_t> decompress_block_zstd(
        const char* compressed_block_data, size_t compressed_block_size, 
        std::optional<uint8_t> dictionary_number = std::optional<int>(), 
        fc::optional<ZSTD_DCtx*> decompression_context_for_reuse = fc::optional<ZSTD_DCtx*>());
  };

} }

