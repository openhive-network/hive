#include <fc/compress/zlib.hpp>

#include <fc/exception/exception.hpp>

#include "miniz.c"

namespace fc
{
  string zlib_compress(const string& in)
  {
    size_t compressed_message_length;
    char* compressed_message = (char*)tdefl_compress_mem_to_heap(in.c_str(), in.size(), &compressed_message_length, TDEFL_WRITE_ZLIB_HEADER | TDEFL_DEFAULT_MAX_PROBES);
    string result(compressed_message, compressed_message_length);
    free(compressed_message);
    return result;
  }

  string zip_decompress(const string& in)
  {
    mz_zip_archive archive={0};
    auto status = mz_zip_reader_init_mem(&archive, in.data(), in.size(), 0);

    FC_ASSERT(status == MZ_TRUE, "mz_zip_reader_init_mem failed");

    mz_uint fCount = mz_zip_reader_get_num_files(&archive);

    FC_ASSERT(fCount == 1, "Single file archive is only supported");

    size_t decompressed_message_length = 0;
    char* decompressed_message = (char*)mz_zip_reader_extract_to_heap(&archive, 0, &decompressed_message_length, 0);

    string result(decompressed_message, decompressed_message_length);
    mz_free(decompressed_message);

    return result;
  }
}
