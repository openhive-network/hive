#pragma once
#include <steem/chain/database.hpp>
#include <steem/protocol/asset_symbol.hpp>

#ifdef HIVE_ENABLE_SMT

namespace hive { namespace chain {

   void replenish_nai_pool( database& db );
   void remove_from_nai_pool( database &db, const asset_symbol_type& a );

} } // hive::chain

#endif
