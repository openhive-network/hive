#pragma once
#include <hive/chain/database.hpp>
#include <hive/protocol/asset_symbol.hpp>

#ifdef HIVE_ENABLE_SMT

namespace hive { namespace chain {

  void replenish_nai_pool( database& db );
  void remove_from_nai_pool( database &db, const asset_symbol_type& a );

} } // hive::chain

#endif
