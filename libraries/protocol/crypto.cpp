
#include <hive/protocol/crypto.hpp>

namespace hive { namespace protocol {

digest_type sig_digest( const transaction& trx, const chain_id_type& chain_id, hive::protocol::pack_type pack )
{
  digest_type::encoder enc;

  hive::protocol::serialization_mode_controller::pack_guard guard( pack );
  fc::raw::pack( enc, chain_id );
  fc::raw::pack( enc, trx );

  return enc.result();
}

} } // end namespace hive::protocol
