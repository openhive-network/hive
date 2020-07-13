#include <hive/chain/util/hf23_helper.hpp>

namespace hive { namespace chain {

void hf23_helper::gather_balance( hf23_items& source, const std::string& name, const HIVE_asset& balance, const HBD_asset& hbd_balance )
{
  source.emplace( name, balance, hbd_balance );
}

} } // namespace hive::chain
