#include <steem/chain/util/hf23_helper.hpp>

namespace steem { namespace chain {

void hf23_helper::gather_balance( hf23_items& source, const std::string& name, const asset& balance, const asset& sbd_balance )
{
   source.emplace( hf23_item{ name, balance, sbd_balance } );
}

} } // namespace steem::chain
