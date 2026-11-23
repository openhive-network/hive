#include <hive/chain/util/seeded_hash.hpp>

#include <fc/crypto/rand.hpp>

namespace hive { namespace chain { namespace util {

seeded_hash::seeded_hash()
{
  uint64_t key[2];
  fc::rand_bytes( reinterpret_cast< char* >( key ), sizeof( key ) );
  _k0 = key[0];
  _k1 = key[1];
}

} } } // hive::chain::util
