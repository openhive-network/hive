#pragma once

#include <hive/protocol/transaction.hpp>

namespace hive { namespace chain {

using hive::protocol::pack_type;
using hive::protocol::signed_transaction;

class signed_transaction_transporter
{
  private:

    using t_packed_trx = std::vector<char>;

    t_packed_trx  packed_trx;
    pack_type     pack = pack_type::legacy;

    void fill();

  public:

    const signed_transaction& trx;

    signed_transaction_transporter( const signed_transaction& trx );
};

} } // hive::chain
