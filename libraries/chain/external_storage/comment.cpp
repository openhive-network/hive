#include <hive/chain/external_storage/comment.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/comment_object.hpp>

namespace hive { namespace chain {

  comment::comment( const comment_object* shm ): shm( shm )
  {
  }

  comment::comment( const std::shared_ptr<comment_object>& external ): external( external )
  {
    if( external )
      shm = &( *external );
  }

  const comment_object::id_type comment::get_id() const
  {
    return shm->get_id();
  }

  const comment_object::author_and_permlink_hash_type& comment::get_author_and_permlink_hash() const
  {
    return shm->get_author_and_permlink_hash();
  }

  const comment_id_type comment::get_parent_id() const
  {
    return shm->get_parent_id();
  }

  const bool comment::is_root() const
  {
    return shm->is_root();
  }

  const uint16_t comment::get_depth() const
  {
    return shm->get_depth();
  }

  comment::operator bool() const
  {
    return shm;
  }

  const comment_object& comment::operator*() const
  {
    return *shm;
  }

} } // hive::chain
