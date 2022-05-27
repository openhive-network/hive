#include <hive/chain/signed_transaction_transporter.hpp>

namespace hive { namespace chain {

  void signed_transaction_transporter::fill()
  {
    FC_TODO( "Fill members" )
  }

  signed_transaction_transporter::signed_transaction_transporter( const signed_transaction& trx ): trx( trx )
  {
    fill();
  }

} } // hive::chain
