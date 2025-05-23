
#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/comment_object.hpp>

namespace hive { namespace chain {

class comment
{
  const comment_object*           ptr = nullptr;
  std::shared_ptr<comment_object> external;

public:
  comment(){}
  comment( const comment_object* from_shm ) : ptr( from_shm ) {}
  comment( const std::shared_ptr<comment_object>& from_archive )
    : ptr( from_archive.get() ), external( from_archive ) {}

  const comment_object::id_type get_id() const { return ptr->get_id(); }

  const comment_object::author_and_permlink_hash_type& get_author_and_permlink_hash() const
  {
    return ptr->get_author_and_permlink_hash();
  }

  const comment_id_type get_parent_id() const { return ptr->get_parent_id(); }

  const bool is_root() const { return ptr->is_root(); }

  const uint16_t get_depth() const { return ptr->get_depth(); }

  operator bool() const { return ptr != nullptr; }

  const comment_object& operator*() const { return *ptr; }
  const comment_object* get() const { return ptr; }
};


} } // hive::chain
