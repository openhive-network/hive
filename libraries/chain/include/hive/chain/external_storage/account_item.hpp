
#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/account_object.hpp>

namespace hive { namespace chain {

template<typename Item>
class account_item
{
  const Item*           ptr = nullptr;
  std::shared_ptr<Item> external;

public:
  account_item(){}
  account_item( const Item* from_shm ) : ptr( from_shm ) {}
  account_item( const std::shared_ptr<Item>& from_archive )
    : ptr( from_archive.get() ), external( from_archive ) {}

  const Item& operator*() const { return *ptr; }
  Item& operator*() { return const_cast<Item&>( *ptr ); }

  bool is_shm() const { return external == nullptr; }
};

using account_metadata = account_item<account_metadata_object>;
using account_authority = account_item<account_authority_object>;

} } // hive::chain
