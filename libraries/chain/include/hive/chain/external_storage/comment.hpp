#pragma once
#include <memory>

namespace hive { namespace chain {

class comment_object;

class comment
{
  const comment_object*           ptr = nullptr;
  std::shared_ptr<comment_object> external;

public:
  comment() = default;
  comment( const comment_object* from_shm ) : ptr( from_shm ) {}
  comment( const std::shared_ptr<comment_object>& from_archive )
    : ptr( from_archive.get() ), external( from_archive ) {}

  // Forwarding methods to comment_object (needed for test compatibility)
  // Implementations are at the end of this file after comment_object is fully defined
  inline auto get_id() const;
  inline auto get_author_and_permlink_hash() const;
  inline auto get_parent_id() const;
  inline auto is_root() const;
  inline auto get_depth() const;

  operator bool() const { return ptr != nullptr; }

  const comment_object& operator*() const { return *ptr; }
  const comment_object* operator->() const { return ptr; }
  const comment_object* get() const { return ptr; }
};


} } // hive::chain

// Include comment_object header after comment class declaration to enable forwarding methods
#include <hive/chain/detail/state/comment_object.hpp>

namespace hive { namespace chain {

// Inline implementations of forwarding methods
inline auto comment::get_id() const
{
  return ptr->get_id();
}

inline auto comment::get_author_and_permlink_hash() const
{
  return ptr->get_author_and_permlink_hash();
}

inline auto comment::get_parent_id() const
{
  return ptr->get_parent_id();
}

inline auto comment::is_root() const
{
  return ptr->is_root();
}

inline auto comment::get_depth() const
{
  return ptr->get_depth();
}

} } // hive::chain
