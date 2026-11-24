#include <hive/chain/util/seeded_hash.hpp>

#include <fc/crypto/rand.hpp>

namespace hive { namespace chain { namespace util {

seeded_hash::seeded_hash()
{
  fc::rand_bytes( reinterpret_cast< char* >( &_key ), sizeof( _key ) );
}

} } } // hive::chain::util
