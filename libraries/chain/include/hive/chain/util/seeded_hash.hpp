#pragma once

#include <hive/siphash/siphash.hpp>

#include <cstddef>
#include <cstdint>

namespace hive { namespace chain { namespace util {

/**
  * SipHash-1-3 under a random key drawn when the object is constructed.
  *
  * Hashers of hashed indexes over author-chosen keys (permlinks, account names) hold one, so an author cannot pick
  * keys that fall into one bucket: placement depends on a key nobody else knows. A hasher is stored by value inside
  * its index, so an index in shared memory keeps its key across restart and reopen, while every newly created index
  * (new state, state rebuilt from a snapshot) gets a new one. Copies share the key of the original.
  *
  * Bucket order of such an index is therefore node-specific: it may only be searched, never iterated by anything
  * that affects consensus or any other output that has to be the same on every node.
  */
class seeded_hash
{
public:
  /// draws the key from a cryptographic random source; throws when that source fails
  seeded_hash();

  uint64_t operator()( const void* data, size_t length )const { return siphash::siphash13( _key, data, length ); }

private:
  siphash::key _key;
};

} } } // hive::chain::util
