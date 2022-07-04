#pragma once

#include <fc/optional.hpp>

namespace hive { namespace chain { namespace detail {

enum class block_flags {
  uncompressed = 0,
  zstd = 1
};
struct block_attributes_t {
  block_flags flags = block_flags::uncompressed;
  fc::optional<uint8_t> dictionary_number;
};

inline std::pair<uint64_t, block_attributes_t> split_block_start_pos_with_flags(uint64_t block_start_pos_with_flags)
{
  block_attributes_t attributes;
  attributes.flags = (block_flags)(block_start_pos_with_flags >> 63);
  if(block_start_pos_with_flags & 0x0100000000000000ull)
    attributes.dictionary_number = (uint8_t)((block_start_pos_with_flags >> 48) & 0xff);
  return std::make_pair(block_start_pos_with_flags & 0x0000ffffffffffffull, attributes);
}

inline uint64_t combine_block_start_pos_with_flags(uint64_t block_start_pos, block_attributes_t attributes)
{
  return ((uint64_t)attributes.flags << 63) |
    (attributes.dictionary_number ? 0x0100000000000000ull : 0) |
    ((uint64_t)attributes.dictionary_number.value_or(0) << 48) |
    block_start_pos;
}

}}}

FC_REFLECT_ENUM(hive::chain::detail::block_flags, (uncompressed)(zstd))
FC_REFLECT(hive::chain::detail::block_attributes_t, (flags)(dictionary_number))
