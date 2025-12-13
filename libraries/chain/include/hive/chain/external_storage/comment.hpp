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

  operator bool() const { return ptr != nullptr; }

  const comment_object& operator*() const { return *ptr; }
  const comment_object* operator->() const { return ptr; }
  const comment_object* get() const { return ptr; }
};


} } // hive::chain
