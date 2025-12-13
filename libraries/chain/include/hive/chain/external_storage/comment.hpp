
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

  operator bool() const { return ptr != nullptr; }

  const comment_object& operator*() const { return *ptr; }
  const comment_object* operator->() const { return ptr; }
  const comment_object* get() const { return ptr; }
};


} } // hive::chain
