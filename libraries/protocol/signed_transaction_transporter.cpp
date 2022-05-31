#include <hive/protocol/signed_transaction_transporter.hpp>

#include <hive/protocol/buffer_type_pack.hpp>

#include <hive/protocol/misc_utilities.hpp>

namespace hive { namespace protocol {

  void signed_transaction_transporter::fill()
  {
    hive::protocol::serialization_mode_controller::pack_guard guard( pack );
    fc::raw::pack_to_buffer( packed_trx, trx );
  }

  signed_transaction_transporter::signed_transaction_transporter( const signed_transaction& trx, pack_type pack )
                                : pack( pack ), trx( const_cast<signed_transaction&>( trx ) )
  {
    fill();
  }

  signed_transaction_transporter::signed_transaction_transporter( signed_transaction& trx, pack_type pack )
                                : pack( pack ), trx( trx )
  {
    fill();
  }

  signed_transaction_transporter::signed_transaction_transporter(const signed_transaction_transporter& obj )
                                : pack( obj.pack ), packed_trx( obj.packed_trx ), trx( obj.trx )
  {
  }

  signed_transaction_transporter& signed_transaction_transporter::operator=( signed_transaction_transporter&& obj ) noexcept
  {
    pack = obj.pack;

    trx = std::move( obj.trx );
    packed_trx = std::move( obj.packed_trx );

    return *this;
  }

  signed_transaction_transporter& signed_transaction_transporter::operator=( const signed_transaction_transporter& obj )
  {
    pack = obj.pack;

    trx = obj.trx;
    packed_trx = obj.packed_trx;

    return *this;
  }

  pack_type signed_transaction_transporter::get_pack() const
  {
    return pack;
  }

  const signed_transaction_transporter::t_packed_trx& signed_transaction_transporter::get_packed_trx() const
  {
    return packed_trx;
  }

} } // hive::chain
