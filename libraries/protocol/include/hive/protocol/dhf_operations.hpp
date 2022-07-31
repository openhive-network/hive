#pragma once
#include <hive/protocol/base.hpp>
#include <hive/protocol/operation_util.hpp>
#include <hive/protocol/asset.hpp>


namespace hive { namespace protocol {

struct create_proposal_operation : public base_operation
{
  account_name_type creator;
  /// Account to be paid if given proposal has sufficient votes.
  account_name_type receiver;

  time_point_sec start_date;
  time_point_sec end_date;

  /// Amount of HBD to be daily paid to the `receiver` account.
  asset daily_pay;

  string subject;

  /// Given link shall be a valid permlink. Must be posted by creator the receiver.
  string permlink;

  extensions_type extensions;

  void validate()const;

  void get_required_active_authorities( flat_set<account_name_type>& a )const { a.insert( creator ); }
};

struct update_proposal_end_date
{
  time_point_sec end_date;
};

typedef static_variant<
        void_t,
        update_proposal_end_date
        > update_proposal_extension;

typedef flat_set< update_proposal_extension > update_proposal_extensions_type;

struct update_proposal_operation : public base_operation
{
  int64_t proposal_id;
  account_name_type creator;
  /// Amount of HBDs to be daily paid to the `receiver` account, if updated, has to be lower or equal to the current amount
  asset daily_pay;
  string subject;
  /// Given link shall be a valid permlink. Must be posted by creator or the receiver.
  string permlink;
  update_proposal_extensions_type extensions;

  void validate()const;

  void get_required_active_authorities( flat_set<account_name_type>& a )const { a.insert( creator ); }
};

struct update_proposal_votes_operation : public base_operation
{
  account_name_type voter;

  /// IDs of proposals to vote for/against. Nonexisting IDs are ignored.
  flat_set_ex<int64_t> proposal_ids;

  bool approve = false;

  extensions_type extensions;

  void validate()const;

  void get_required_active_authorities( flat_set<account_name_type>& a )const { a.insert( voter ); }
};

/** Allows to remove proposals specified by given IDs.
    Operation can be performed only by proposal owner. 
*/
struct remove_proposal_operation : public base_operation
{
  account_name_type proposal_owner;

  /// IDs of proposals to be removed. Nonexisting IDs are ignored.
  flat_set_ex<int64_t> proposal_ids;

  extensions_type extensions;

  void validate() const;

  void get_required_active_authorities(flat_set<account_name_type>& a)const { a.insert(proposal_owner); }
};

} } // hive::protocol

namespace fc {

  namespace raw {
      template<typename Stream, typename T>
      inline void pack( Stream& s, const flat_set_ex<T>& value ) {
        pack( s, static_cast< const flat_set<T>& >( value ) );
      }

      template<typename Stream, typename T>
      inline void unpack( Stream& s, flat_set_ex<T>& value, uint32_t depth ) {
        unpack( s, static_cast< flat_set<T>& >( value ), depth );
      }

  }

  template<typename T>
  void to_variant( const flat_set_ex<T>& var,  variant& vo )
  {
    to_variant( static_cast< const flat_set<T>& >( var ), vo );
  }

  template<typename T>
  void from_variant( const variant& var, flat_set_ex<T>& vo )
  {
    const variants& vars = var.get_array();
    vo.clear();
    vo.reserve( vars.size() );
    for( auto itr = vars.begin(); itr != vars.end(); ++itr )
    {
      //Items from variant have to be sorted
      T tmp = itr->as<T>();
      if( !vo.empty() )
      {
        T last = *( vo.rbegin() );
        FC_ASSERT( tmp > last, "Items should be unique and sorted" );
      }

      vo.insert( tmp );
    }
  }
  
}

namespace fc
{
  using hive::protocol::update_proposal_extension;
  template<>
  struct serialization_functor< update_proposal_extension >
  {
    bool operator()( const fc::variant& v, update_proposal_extension& s ) const
    {
      return extended_serialization_functor< update_proposal_extension >().serialize( v, s );
    }
  };

  template<>
  struct variant_creator_functor< update_proposal_extension >
  {
    template<typename T>
    fc::variant operator()( const T& v ) const
    {
      return extended_variant_creator_functor< update_proposal_extension >().create( v );
    }
  };
}

FC_REFLECT_TYPENAME( hive::protocol::update_proposal_extension )

FC_REFLECT( hive::protocol::create_proposal_operation, (creator)(receiver)(start_date)(end_date)(daily_pay)(subject)(permlink)(extensions) )
FC_REFLECT( hive::protocol::update_proposal_operation, (proposal_id)(creator)(daily_pay)(subject)(permlink)(extensions))
FC_REFLECT( hive::protocol::update_proposal_votes_operation, (voter)(proposal_ids)(approve)(extensions) )
FC_REFLECT( hive::protocol::remove_proposal_operation, (proposal_owner)(proposal_ids)(extensions) )
FC_REFLECT( hive::protocol::update_proposal_end_date, (end_date) )

