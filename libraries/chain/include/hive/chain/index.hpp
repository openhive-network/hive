#pragma once

#include <hive/schema/schema.hpp>
#include <hive/protocol/schema_types.hpp>
#include <hive/chain/schema_types.hpp>

#include <hive/chain/database.hpp>

namespace hive { namespace chain {

using hive::schema::abstract_schema;

struct index_info
  : public chainbase::index_extension
{
  index_info();
  virtual ~index_info();
  virtual std::shared_ptr< abstract_schema > get_schema() = 0;
};

template< typename MultiIndexType >
struct index_info_impl
  : public index_info
{
  typedef typename MultiIndexType::value_type value_type;

  index_info_impl()
    : _schema( hive::schema::get_schema_for_type< value_type >() ) {}
  virtual ~index_info_impl() {}

  virtual std::shared_ptr< abstract_schema > get_schema() override
  {   return _schema;   }

  std::shared_ptr< abstract_schema > _schema;
};

template< typename MultiIndexType >
void _add_index_impl( database& db )
{
  db.add_index< MultiIndexType >();
  std::shared_ptr< chainbase::index_extension > ext =
    std::make_shared< index_info_impl< MultiIndexType > >();
  db.add_index_extension< MultiIndexType >( ext );
  db.register_new_type<MultiIndexType>();
}

template< typename MultiIndexType >
void add_core_index( database& db )
{
  _add_index_impl< MultiIndexType >( db );
}

template< typename MultiIndexType >
void add_plugin_index( database& db )
{
  db._plugin_index_signal.connect( [&db](){ _add_index_impl< MultiIndexType >(db); } );
}

} }

#define HIVE_ADD_CORE_INDEX(db, index_name) \
  hive::chain::add_core_index< index_name >( db )

#define HIVE_ADD_PLUGIN_INDEX(db, index_name) \
  hive::chain::add_plugin_index< index_name >( db )
