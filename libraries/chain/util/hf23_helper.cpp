#include <hive/chain/util/hf23_helper.hpp>

namespace hive { namespace chain {

void hf23_helper::gather_balance( hf23_items& source, const std::string& name, const asset& balance, const asset& hbd_balance )
{
  source.emplace( hf23_item{ name, balance, hbd_balance } );
}

} } // namespace hive::chain
