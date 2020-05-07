
#pragma once

#include <hive/schema/abstract_schema.hpp>
#include <hive/schema/schema_impl.hpp>

#include <hive/protocol/types.hpp>

namespace hive { namespace schema { namespace detail {

//////////////////////////////////////////////
// account_name_type                        //
//////////////////////////////////////////////

struct schema_account_name_type_impl
   : public abstract_schema
{
   HIVE_SCHEMA_CLASS_BODY( schema_account_name_type_impl )
};

}

template<>
struct schema_reflect< hive::protocol::account_name_type >
{
   typedef detail::schema_account_name_type_impl           schema_impl_type;
};

} }

namespace fc {

template<>
struct get_typename< hive::protocol::account_name_type >
{
   static const char* name()
   {
      return "hive::protocol::account_name_type";
   }
};

}
