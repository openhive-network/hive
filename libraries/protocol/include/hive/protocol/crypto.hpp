
#pragma once

#include <hive/protocol/types.hpp>
#include <hive/protocol/transaction.hpp>

namespace hive { namespace protocol {

  digest_type sig_digest( const transaction& trx, const chain_id_type& chain_id, pack_type pack );
  digest_type digest( const transaction& trx );

} } // end namespace hive::protocol
