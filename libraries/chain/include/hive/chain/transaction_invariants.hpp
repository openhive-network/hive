#pragma once
#include <hive/protocol/transaction.hpp>

namespace hive { namespace chain {

/**
 * Supplement for signed_transaction.
 * Holds data on transaction that is independent of state.
 */
class transaction_invariants
{
public:
  explicit transaction_invariants( const protocol::transaction_id_type& _id ) : id( _id ){}

  void set_static_validation( bool _valid = true ) const { valid = _valid; }
  void store_public_keys( flat_set< protocol::public_key_type >&& _keys ) const
  {
    signature_public_keys = std::move( _keys );
  }

  const protocol::transaction_id_type& get_id() const { return id; }
  bool statically_validated() const { return valid; }
  const flat_set< protocol::public_key_type >& get_public_keys() const { return signature_public_keys; }
  bool empty() const { return signature_public_keys.empty(); }

private:
  protocol::transaction_id_type id;
  mutable flat_set< protocol::public_key_type > signature_public_keys;
  mutable bool valid = false;
};

inline bool operator< ( const transaction_invariants& _l, const transaction_invariants& _r )
{
  return _l.get_id() < _r.get_id();
}
inline bool operator< ( const transaction_id_type& _l, const transaction_invariants& _r )
{
  return _l < _r.get_id();
}
inline bool operator< ( const transaction_invariants& _l, const transaction_id_type& _r )
{
  return _l.get_id() < _r;
}

} } //hive::chain
