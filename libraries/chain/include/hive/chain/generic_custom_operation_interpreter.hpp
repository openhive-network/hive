
#pragma once

#include <hive/protocol/schema_types.hpp>
#include <hive/chain/schema_types.hpp>
#include <hive/schema/schema.hpp>

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/operation_util_impl.hpp>
#include <hive/protocol/types.hpp>

#include <hive/chain/evaluator.hpp>
#include <hive/chain/evaluator_registry.hpp>
#include <hive/chain/custom_operation_interpreter.hpp>

#include <fc/variant.hpp>

#include <string>
#include <vector>

namespace hive { namespace chain {

using protocol::operation;
using protocol::authority;
using protocol::account_name_type;
using protocol::custom_id_type;

class database;

std::string legacy_custom_name_from_type( const std::string& type_name );

struct get_legacy_custom_operation_name
{
  string& name;
  get_legacy_custom_operation_name( string& dv )
    : name( dv ) {}

  typedef void result_type;
  template< typename T > void operator()( const T& v )const
  {
    name = legacy_custom_name_from_type( fc::get_typename< T >::name() );
  }
};

struct get_custom_operation_name
{
  string& name;
  get_custom_operation_name( string& n )
    : name( n ) {}

  typedef void result_type;

  template< typename T > void operator()( const T& v )const
  {
    name = fc::trim_typename_namespace( fc::get_typename< T >::name() );
  }
};

template< typename CustomOperationType >
void custom_op_from_variant( const fc::variant& var, CustomOperationType& vo, const database& db )
{
  static std::map<string,int64_t> to_legacy_tag = []()
  {
    std::map<string,int64_t> name_map;
    for( int i = 0; i < CustomOperationType::count(); ++i )
    {
      string n;
      get_legacy_custom_operation_name  visitor(n);
      CustomOperationType tmp(i, visitor);
      name_map[n] = i;
    }
    return name_map;
  }();

  static std::map< string, int64_t > to_tag = []()
  {
    std::map< string, int64_t > name_map;
    for( int i = 0; i < CustomOperationType::count(); ++i )
    {
      string n;
      get_custom_operation_name  visitor(n);
      CustomOperationType tmp(i, visitor);
      name_map[n] = i;
    }
    return name_map;
  }();

  if( var.is_array() ) // legacy serialization
  {
    auto ar = var.get_array();
    if( ar.size() != 2 )
    {
      FC_ASSERT( !db.is_in_control(), "Expected pair of values: [ operation_name_or_id, operation ]" );
      if( ar.size() < 2 )
      {
        dlog( "Incomplete custom operation ignored @${b} ${var}", ( "b", db.head_block_num() + 1 )( var ) );
        return;
      }
      dlog( "Dangling elements of custom operation ignored @${b} ${var}", ( "b", db.head_block_num() + 1 )( var ) );
    }

    fc::to_static_variant visitor( ar[1] );
    if( ar[0].is_uint64() )
    {
      vo = CustomOperationType( ar[0].as_uint64(), visitor );
    }
    else
    {
      auto itr = to_legacy_tag.find(ar[0].as_string());
      FC_ASSERT( itr != to_legacy_tag.end(), "Invalid operation name: ${n}", ("n", ar[0]) );
      vo = CustomOperationType( itr->second, visitor );
    }
  }
  else // new serialization
  {
    FC_ASSERT( var.is_object(), "Input data have to treated as object." );
    auto v_object = var.get_object();

    FC_ASSERT( v_object.contains( "type" ), "Type field doesn't exist." );
    FC_ASSERT( v_object.contains( "value" ), "Value field doesn't exist." );
    if( v_object.size() != 2 )
    {
      FC_ASSERT( !db.is_in_control(), "Expected pair of values: { \"type\":operation_name_or_id, \"value\":operation }" );
      dlog( "Dangling elements of custom operation ignored @${b} ${var}", ( "b", db.head_block_num() + 1 )( var ) );
    }

    int64_t which = -1;

    if( v_object[ "type" ].is_integer() )
    {
      which = v_object[ "type" ].as_int64();
    }
    else
    {
      auto itr = to_tag.find( v_object[ "type" ].as_string() );
      FC_ASSERT( itr != to_tag.end(), "Invalid object name: ${n}", ("n", v_object[ "type" ]) );
      which = itr->second;
    }

    fc::to_static_variant visitor( v_object[ "value" ] );
    vo = CustomOperationType( which, visitor );
  }
}


template< typename OpType >
class generic_custom_operation_notification : public custom_operation_notification
{
public:
  generic_custom_operation_notification( const custom_id_type& cid, const OpType& o )
                         : custom_operation_notification(cid), op(o) {}

  const OpType& op;
};

template< typename OpType >
const OpType& custom_operation_notification::get_op()const
{
  const OpType* find_op = find_get_op< OpType >();
  FC_ASSERT( find_op );
  return *find_op;
}

template< typename OpType >
const OpType* custom_operation_notification::find_get_op()const
{
  const generic_custom_operation_notification< OpType >* generic_this = dynamic_cast< const generic_custom_operation_notification< OpType >* >( this );
  const OpType* result = nullptr;
  if( generic_this )
    result = &(generic_this->op);
  return result;
}


template< typename CustomOperationType >
class generic_custom_operation_interpreter
  : public custom_operation_interpreter, public evaluator_registry< CustomOperationType >
{
  public:
    generic_custom_operation_interpreter( database& db, const custom_id_type& cid )
      : evaluator_registry< CustomOperationType >(db), custom_id(cid) {}
    virtual ~generic_custom_operation_interpreter() = default;

    void apply_operations( const vector< CustomOperationType >& custom_operations, const operation& outer_o )
    {
      auto plugin_session = this->_db.start_undo_session();

      flat_set<account_name_type> outer_active;
      flat_set<account_name_type> outer_owner;
      flat_set<account_name_type> outer_posting;
      flat_set<account_name_type> outer_witness;
      std::vector< authority >    outer_other;

      flat_set<account_name_type> inner_active;
      flat_set<account_name_type> inner_owner;
      flat_set<account_name_type> inner_posting;
      flat_set<account_name_type> inner_witness;
      std::vector< authority >    inner_other;

      operation_get_required_authorities( outer_o, outer_active, outer_owner, outer_posting, outer_witness, outer_other );
      for( const CustomOperationType& inner_o : custom_operations )
      {
        operation_validate( inner_o );
        operation_get_required_authorities( inner_o, inner_active, inner_owner, inner_posting, inner_witness, inner_other );
      }

      FC_ASSERT( inner_owner == outer_owner );
      FC_ASSERT( inner_active == outer_active );
      FC_ASSERT( inner_posting == outer_posting );
      FC_ASSERT( inner_witness == outer_witness );
      FC_ASSERT( inner_other == outer_other );

      for( const CustomOperationType& inner_o : custom_operations )
      {
        // gcc errors if this-> is not here
        // error message is "declarations in dependent base are not found by unqualified lookup"
        generic_custom_operation_notification< CustomOperationType > cnote( custom_id, inner_o );
        this->_db.notify_pre_apply_custom_operation( cnote );
        this->get_evaluator( inner_o ).apply( inner_o );
        this->_db.notify_post_apply_custom_operation( cnote );
        ++cnote.op_in_custom;
      }

      plugin_session.squash();
    }

    virtual void apply( const protocol::custom_json_operation& outer_o ) override
    {
      try
      {
        fc::variant v = fc::json::from_string( outer_o.json, (this->_db.is_in_control() ? fc::json::format_validation_mode::full : fc::json::format_validation_mode::relaxed) );

        std::vector< CustomOperationType > custom_operations;
        if( v.is_array() && v.size() > 0 && v.get_array()[0].is_array() )
        {
          // it looks like a list
          for( auto& o : v.get_array() )
          {
            custom_operations.emplace_back();
            custom_op_from_variant( o, custom_operations.back(), this->_db );
          }
        }
        else
        {
          custom_operations.emplace_back();
          custom_op_from_variant( v, custom_operations[0], this->_db );
        }

        apply_operations( custom_operations, operation( outer_o ) );
      } FC_CAPTURE_AND_RETHROW( (outer_o) )
    }

    virtual void apply( const protocol::custom_binary_operation& outer_o ) override
    {
      try
      {
        vector< CustomOperationType > custom_operations;

        try
        {
          fc::raw::unpack_from_vector( outer_o.data, custom_operations );
        }
        catch ( fc::exception& )
        {
          CustomOperationType tmp;
          fc::raw::unpack_from_vector(outer_o.data, tmp);
          custom_operations.push_back(tmp);
        }

        apply_operations( custom_operations, operation( outer_o ) );
      }
      FC_CAPTURE_AND_RETHROW( (outer_o) )
    }

    virtual std::shared_ptr< hive::schema::abstract_schema > get_operation_schema() override
    {
      return hive::schema::get_schema_for_type< CustomOperationType >();
    }

    virtual custom_id_type get_custom_id() override
    {
      return custom_id;
    }

  private:
    custom_id_type custom_id;
};

} }
