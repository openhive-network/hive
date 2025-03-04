#include <hive/plugins/state_snapshot/dumper_comment_data.hpp>

#include <hive/chain/comment_object.hpp>
#include <memory>

namespace hive {

  dumper_comment_data::dumper_comment_data()
  {
    c = std::make_shared<xxx::core>();
  }

  void dumper_comment_data::write( const void* obj )
  {
    const hive::chain::comment_object* _comment = reinterpret_cast<const hive::chain::comment_object*>( obj );
    FC_ASSERT( _comment );
    //ilog( _comment->get_author_and_permlink_hash().str() );

    c->storeHash( _comment->get_author_and_permlink_hash() );
  }

}
