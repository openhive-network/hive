#include <hive/chain/block_log_compression.hpp>
#include <hive/protocol/config.hpp>

#include <hive/chain/block_compression_dictionaries.hpp>

#include <fc/exception/exception.hpp>

#include <boost/scope_exit.hpp>

#ifndef ZSTD_STATIC_LINKING_ONLY
# define ZSTD_STATIC_LINKING_ONLY
#endif
#include <zstd.h>

namespace hive { namespace chain {

std::tuple<std::unique_ptr<char[]>, size_t> compress_block_zstd_helper(const char* uncompressed_block_data, 
                                                                         size_t uncompressed_block_size,
                                                                         std::optional<uint8_t> dictionary_number,
                                                                         ZSTD_CCtx* compression_context,
                                                                         fc::optional<int> compression_level)
  {
    if (dictionary_number)
    {
      ZSTD_CDict* compression_dictionary = get_zstd_compression_dictionary(*dictionary_number, compression_level.value_or(ZSTD_CLEVEL_DEFAULT));
      size_t ref_cdict_result = ZSTD_CCtx_refCDict(compression_context, compression_dictionary);
      if (ZSTD_isError(ref_cdict_result))
        FC_THROW("Error loading compression dictionary into context");
    }
    else
    {
      ZSTD_CCtx_setParameter(compression_context, ZSTD_c_compressionLevel, compression_level.value_or(ZSTD_CLEVEL_DEFAULT));
    }

    // prevent zstd from adding a 4-byte magic number at the beginning of each block, it serves no purpose here other than wasting 4 bytes
    ZSTD_CCtx_setParameter(compression_context, ZSTD_c_format, ZSTD_f_zstd1_magicless);
    // prevent zstd from adding the decompressed size at the beginning of each block.  For most blocks (those >= 256 bytes),
    // this will save a byte in the encoded output.  For smaller blocks, it does no harm.
    ZSTD_CCtx_setParameter(compression_context, ZSTD_c_contentSizeFlag, 0);
    // prevent zstd from encoding the number of the dictionary used for encoding the block.  We store this in the position/flags, so 
    // we can save another byte here
    ZSTD_CCtx_setParameter(compression_context, ZSTD_c_dictIDFlag, 0);

    // tell zstd not to add a 4-byte checksum at the end of the output.  This is currently the default, so should have no effect
    ZSTD_CCtx_setParameter(compression_context, ZSTD_c_checksumFlag, 0);

    size_t zstd_max_size = ZSTD_compressBound(uncompressed_block_size);
    std::unique_ptr<char[]> zstd_compressed_data(new char[zstd_max_size]);

    size_t zstd_compressed_size = ZSTD_compress2(compression_context, 
                                                 zstd_compressed_data.get(), zstd_max_size,
                                                 uncompressed_block_data, uncompressed_block_size);

    if (ZSTD_isError(zstd_compressed_size))
      FC_THROW("Error compressing block with zstd");


    return std::make_tuple(std::move(zstd_compressed_data), zstd_compressed_size);
  }

  /* static */ std::tuple<std::unique_ptr<char[]>, size_t> block_log_compression::decompress_raw_block(const char* raw_block_data, size_t raw_block_size, block_attributes_t attributes)
  {
    try
    {
      switch (attributes.flags)
      {
      case block_flags::zstd:
        return block_log_compression::decompress_block_zstd(raw_block_data, raw_block_size, attributes.dictionary_number);
      case block_flags::uncompressed:
        {
          std::unique_ptr<char[]> block_data_copy(new char[raw_block_size]);
          memcpy(block_data_copy.get(), raw_block_data, raw_block_size);
          return std::make_tuple(std::move(block_data_copy), raw_block_size);
        }
      default:
        FC_THROW("Unrecognized block_flags in block log");
      }
    }
    FC_LOG_AND_RETHROW()
  }

  /* static */ std::tuple<std::unique_ptr<char[]>, size_t> block_log_compression::decompress_raw_block(std::tuple<std::unique_ptr<char[]>, size_t, block_attributes_t>&& raw_block_data_tuple)
  {
    std::unique_ptr<char[]> raw_block_data = std::get<0>(std::move(raw_block_data_tuple));
    size_t raw_block_size = std::get<1>(raw_block_data_tuple);
    block_attributes_t attributes = std::get<2>(raw_block_data_tuple);

    // avoid a copy if it's uncompressed
    if (attributes.flags == block_flags::uncompressed)
      return std::make_tuple(std::move(raw_block_data), raw_block_size);

    return decompress_raw_block(raw_block_data.get(), raw_block_size, attributes);
  }

  /* static */ std::tuple<std::unique_ptr<char[]>, size_t> block_log_compression::compress_block_zstd(
    const char* uncompressed_block_data, size_t uncompressed_block_size,
    std::optional<uint8_t> dictionary_number, fc::optional<int> compression_level,
    fc::optional<ZSTD_CCtx*> compression_context_for_reuse)
  {
    if (compression_context_for_reuse)
    {
      ZSTD_CCtx_reset(*compression_context_for_reuse, ZSTD_reset_session_and_parameters);
      return compress_block_zstd_helper(uncompressed_block_data, 
                                        uncompressed_block_size,
                                        dictionary_number,
                                        *compression_context_for_reuse,
                                        compression_level);
    }

    ZSTD_CCtx* compression_context = ZSTD_createCCtx();
    BOOST_SCOPE_EXIT(&compression_context) {
      ZSTD_freeCCtx(compression_context);
    } BOOST_SCOPE_EXIT_END

    return compress_block_zstd_helper(uncompressed_block_data, 
                                      uncompressed_block_size,
                                      dictionary_number,
                                      compression_context,
                                      compression_level);
  }

  std::tuple<std::unique_ptr<char[]>, size_t> decompress_block_zstd_helper(const char* compressed_block_data,
                                                                           size_t compressed_block_size,
                                                                           std::optional<uint8_t> dictionary_number,
                                                                           ZSTD_DCtx* decompression_context)
  {
    if (dictionary_number)
    {
      ZSTD_DDict* decompression_dictionary = get_zstd_decompression_dictionary(*dictionary_number);
      size_t ref_ddict_result = ZSTD_DCtx_refDDict(decompression_context, decompression_dictionary);
      if (ZSTD_isError(ref_ddict_result))
        FC_THROW("Error loading decompression dictionary into context");
    }

    // tell zstd not to expect the first four bytes to be a magic number
    ZSTD_DCtx_setParameter(decompression_context, ZSTD_d_format, ZSTD_f_zstd1_magicless);

    std::unique_ptr<char[]> uncompressed_block_data(new char[HIVE_MAX_BLOCK_SIZE]);
    size_t uncompressed_block_size = ZSTD_decompressDCtx(decompression_context,
                                                         uncompressed_block_data.get(), HIVE_MAX_BLOCK_SIZE,
                                                         compressed_block_data, compressed_block_size);
    if (ZSTD_isError(uncompressed_block_size))
      FC_THROW("Error decompressing block with zstd");
    
    FC_ASSERT(uncompressed_block_size <= HIVE_MAX_BLOCK_SIZE);

    /// Return a buffer adjusted to actual block size, since such buffers can be accumulated at caller side...
    std::unique_ptr<char[]> actual_buffer(new char[uncompressed_block_size]);
    memcpy(actual_buffer.get(), uncompressed_block_data.get(), uncompressed_block_size);
    return std::make_tuple(std::move(actual_buffer), uncompressed_block_size);
  }

  /* static */ std::tuple<std::unique_ptr<char[]>, size_t> block_log_compression::decompress_block_zstd(
    const char* compressed_block_data, size_t compressed_block_size,
    std::optional<uint8_t> dictionary_number, fc::optional<ZSTD_DCtx*> decompression_context_for_reuse)
  {
    if (decompression_context_for_reuse)
    {
      ZSTD_DCtx_reset(*decompression_context_for_reuse, ZSTD_reset_session_and_parameters);
      return decompress_block_zstd_helper(compressed_block_data, 
                                          compressed_block_size,
                                          dictionary_number,
                                          *decompression_context_for_reuse);
    }

    ZSTD_DCtx* decompression_context = ZSTD_createDCtx();
    BOOST_SCOPE_EXIT(&decompression_context) {
      ZSTD_freeDCtx(decompression_context);
    } BOOST_SCOPE_EXIT_END

    return decompress_block_zstd_helper(compressed_block_data, 
                                        compressed_block_size,
                                        dictionary_number,
                                        decompression_context);

  }

} } // hive::chain
