#include <core/beekeeper_wallet_base.hpp>

namespace beekeeper {

  boost::signals2::connection beekeeper_wallet_base::connect( on_update_handler&& handler )
  {
    return on_update_signal.connect( handler );
  }

  void beekeeper_wallet_base::on_update()
  {
    on_update_signal( name );
  }
}