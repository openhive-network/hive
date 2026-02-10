#pragma once

#include <cstdint>
#include <limits>

#include <fc/exception/exception.hpp>

using OperationId = uint64_t;
using OperationPositionInBlock = uint32_t;
using OperationBlockNum = uint32_t;

inline OperationId
to_operation_id( OperationBlockNum _block_num, OperationPositionInBlock _pos_in_block ) {
    //msb...........lsb
    // || block | seq ||
    // ||  32b  | 32b ||
    constexpr auto BLOCK_NUM_LIMIT = std::numeric_limits< int32_t >::max(); // ignore complement code bit
    FC_ASSERT(  _block_num <= BLOCK_NUM_LIMIT, "Block num value is larger than 31 bits" );

int64_t result = _block_num;
result <<= 32;
result |= _pos_in_block;

return result;
}

inline OperationPositionInBlock
operation_id_to_pos( OperationId _id ) {
return _id & 0xFFFFFFFF;
}

inline OperationBlockNum
operation_id_to_block_num( OperationId _id ) {
return _id >> 32;
}
