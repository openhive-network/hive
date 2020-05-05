
#pragma once

#include <hive/schema/abstract_schema.hpp>
#include <hive/schema/schema_impl.hpp>

#include <hive/protocol/asset_symbol.hpp>

namespace hive { namespace schema { namespace detail {

//////////////////////////////////////////////
// asset_symbol_type                        //
//////////////////////////////////////////////

struct schema_asset_symbol_type_impl
   : public abstract_schema
{
   HIVE_SCHEMA_CLASS_BODY( schema_asset_symbol_type_impl )
};

}

template<>
struct schema_reflect< hive::protocol::asset_symbol_type >
{
   typedef detail::schema_asset_symbol_type_impl           schema_impl_type;
};

} }
