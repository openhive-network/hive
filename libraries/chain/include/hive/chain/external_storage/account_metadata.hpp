
#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/account_object.hpp>

namespace hive { namespace chain {

class account_metadata
{
  const account_metadata_object*           ptr = nullptr;
  std::shared_ptr<account_metadata_object> external;

public:
  account_metadata(){}
  account_metadata( const account_metadata_object* from_shm ) : ptr( from_shm ) {}
  account_metadata( const std::shared_ptr<account_metadata_object>& from_archive )
    : ptr( from_archive.get() ), external( from_archive ) {}

  const account_name_type get_account_name() const { return ptr->account; }

  const std::string get_json_metadata() const { return ptr->json_metadata.c_str(); }
  const std::string get_posting_json_metadata() const { return ptr->posting_json_metadata.c_str(); }

  //operator bool() const { return ptr != nullptr; }

  //const comment_object& operator*() const { return *ptr; }
  //const comment_object* get() const { return ptr; }
};


} } // hive::chain
