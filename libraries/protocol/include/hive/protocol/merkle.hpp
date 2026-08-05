#pragma once
#include <hive/protocol/types.hpp>

#include <vector>

namespace hive { namespace protocol {

/**
 * Computes the Merkle root of the given digests.
 *
 * The caller is responsible for preparing @p ids (e.g. from transaction merkle
 * digests or from arbitrary state stamps); this function only combines them into
 * the root. The vector is consumed - it is reused as scratch space during the
 * computation. Returns an empty checksum_type when @p ids is empty.
 */
checksum_type calculate_merkle_root( std::vector<digest_type> ids );

} } // hive::protocol
