
#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/account_object.hpp>

namespace hive { namespace chain {

template<typename Object_Type>
class account_item
{
  const Object_Type*           ptr = nullptr;
  std::shared_ptr<Object_Type> external;

public:
  account_item(){}
  account_item( const Object_Type* from_shm ) : ptr( from_shm ) {}
  account_item( const std::shared_ptr<Object_Type>& from_archive )
    : ptr( from_archive.get() ), external( from_archive ) {}

  operator bool() const { return ptr != nullptr; }

  const Object_Type& operator*() const { return *ptr; }
  const Object_Type* get() const { return ptr; }
};

using account           = account_item<account_object>;
using account_metadata  = account_item<account_metadata_object>;
using account_authority = account_item<account_authority_object>;

} } // hive::chain
