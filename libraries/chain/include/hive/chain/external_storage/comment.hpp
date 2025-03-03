
#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/comment_object.hpp>

namespace hive { namespace chain {

  class comment
  {
    private:

      comment_object*                 shm = nullptr;
      std::shared_ptr<comment_object> external;

    public:

      comment(){}
      comment( const comment_object* shm );
      comment( const std::shared_ptr<comment_object>& external );

      const comment_object::id_type get_id() const;

      const comment_object::author_and_permlink_hash_type& get_author_and_permlink_hash() const;

      const comment_id_type get_parent_id() const;

      const bool is_root() const;

      const uint16_t get_depth() const;

      operator bool() const;

      const comment_object& operator*() const;

  };


} } // hive::chain
