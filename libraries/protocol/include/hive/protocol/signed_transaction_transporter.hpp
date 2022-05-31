#pragma once

#include <hive/protocol/transaction.hpp>
#include <fc/io/raw.hpp>

namespace hive { namespace protocol {

using hive::protocol::pack_type;
using hive::protocol::signed_transaction;

class signed_transaction_transporter
{
  private:

    using t_packed_trx = std::vector<char>;

    pack_type     pack = pack_type::hf26;
    t_packed_trx  packed_trx;

    void fill();

  public:

    signed_transaction trx;

    signed_transaction_transporter(){}
    explicit signed_transaction_transporter( const signed_transaction& trx, pack_type pack );
    explicit signed_transaction_transporter( signed_transaction& trx, pack_type pack );

    signed_transaction_transporter(const signed_transaction_transporter& obj );

    signed_transaction_transporter& operator=( signed_transaction_transporter&& obj ) noexcept;
    signed_transaction_transporter& operator=( const signed_transaction_transporter& obj );

    pack_type get_pack() const;
    const t_packed_trx& get_packed_trx() const;
};

} } // hive::protocol

FC_REFLECT( hive::protocol::signed_transaction_transporter, (trx) )

namespace fc { namespace raw {
  
template< typename Stream >
inline void pack( Stream& s, const hive::protocol::signed_transaction_transporter& sym )
{
  hive::protocol::serialization_mode_controller::pack_guard guard( sym.get_pack() );
  pack( s, sym.trx );
}

template< typename Stream >
inline void unpack( Stream& s, hive::protocol::signed_transaction_transporter& value, uint32_t )
{
  unpack( s, value.trx );
}

} }// fc::raw
