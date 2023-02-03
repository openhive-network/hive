#pragma once

#include <type_traits>
#include <set>
#include <functional>

#include <fc/optional.hpp>

#include <hive/protocol/types.hpp>

#include <boost/container/flat_set.hpp>

namespace hive { namespace converter { namespace plugins { namespace iceberg_generate { namespace detail {

namespace hp = hive::protocol;

namespace utils
{
  template< typename T, typename... OneOfTs >
  struct is_one_of;

  template< typename T, typename OneOfT, typename... OneOfTs >
  struct is_one_of<T, OneOfT, OneOfTs...>
  {
    static constexpr bool value = std::is_same<T, OneOfT>::value || is_one_of<T, OneOfTs...>::value;
  };

  template< typename SearchForT >
  struct is_one_of<SearchForT>
  {
    static constexpr bool value = false;
  };

  template< typename T, typename... OneOfTs >
  using enable_if_one_of = typename std::enable_if< is_one_of< T, OneOfTs... >::value, void >::type;

  template< typename T, typename... OneOfTs >
  using enable_if_not_one_of = typename std::enable_if< !is_one_of< T, OneOfTs... >::value, void >::type;
}

class account_name_type_visitor
{
public:
  using result_type = std::set<hp::account_name_type>;

  template< typename T >
  result_type operator()( const T& op );
};

template< typename OpType >
class account_name_type_reflect_visitor
{
private:
  const OpType& op;

  account_name_type_visitor::result_type impacted_accounts;

  void add_account( const hp::account_name_type& acc )
  {
    impacted_accounts.emplace(acc);
  }

public:
  account_name_type_reflect_visitor( const OpType& op )
    : op( op )
  {}

  const account_name_type_visitor::result_type& get_impacted_accounts()const
  {
    return impacted_accounts;
  }

  template<typename Member, class Class, Member (Class::*member)>
  utils::enable_if_not_one_of< Member
    , hp::account_name_type
    , hp::comment_options_extensions_type
    , boost::container::flat_set< hp::account_name_type >
    , hp::pow2_work
#ifdef HIVE_ENABLE_SMT
    , hp::smt_generation_policy
    , hp::smt_emissions_unit
#endif
  >
  operator()( const char* name )
  {
    // Do nothing - we only need account names
  }

#ifdef HIVE_ENABLE_SMT

  template<class Member, class Class, Member (Class::*member)>
  utils::enable_if_one_of<Member, hp::smt_generation_policy>
  operator()( const char* name )
  {
    for( const auto& [acc, unit] : (op.*member).emissions_unit.token_unit )
      add_account(acc);
  }

  template<class Member, class Class, Member (Class::*member)>
  utils::enable_if_one_of<Member, hp::smt_generation_policy>
  operator()( const char* name )
  {
    fc::optional<hp::smt_capped_generation_policy> cpb = (op.*member).which() ? fc::optional<hp::smt_capped_generation_policy>{} : (op.*member).template get<hp::smt_capped_generation_policy>();

    if( cpb.valid() )
    {
      for( const auto& [acc, unit] : cpb.pre_soft_cap_unit.hive_unit )
        add_account(acc);
      for( const auto& [acc, unit] : cpb.pre_soft_cap_unit.token_unit )
        add_account(acc);
      for( const auto& [acc, unit] : cpb.post_soft_cap_unit.hive_unit )
        add_account(acc);
      for( const auto& [acc, unit] : cpb.post_soft_cap_unit.token_unit )
        add_account(acc);
    }
  }

#endif

  template<class Member, class Class, Member (Class::*member)>
  utils::enable_if_one_of<Member, hp::pow2_work>
  operator()( const char* name )
  {
    const auto& input = (op.*member).which() ? (op.*member).template get< hp::equihash_pow >().input : (op.*member).template get< hp::pow2 >().input;

    add_account(input.worker_account);
  }

  template<class Member, class Class, Member (Class::*member)>
  utils::enable_if_one_of<Member, boost::container::flat_set< hp::account_name_type >>
  operator()( const char* name )
  {
    for( const auto& acc : op.*member )
      add_account(acc);
  }

  template<class Member, class Class, Member (Class::*member)>
  utils::enable_if_one_of<Member, hp::comment_options_extensions_type>
  operator()( const char* name )
  {
    for( auto& ext : op.*member )
    {
      fc::optional<hp::comment_payout_beneficiaries> cpb = ext.which() ? fc::optional<hp::comment_payout_beneficiaries>{} : ext.template get<hp::comment_payout_beneficiaries>();

      if( cpb.valid() )
        for( auto& route : cpb->beneficiaries )
          add_account(route.account);
    }
  }

  template<class Member, class Class, Member (Class::*member)>
  utils::enable_if_one_of<Member, hp::account_name_type>
  operator()( const char* name )
  {
    add_account(op.*member);
  }
};

template< typename T >
account_name_type_visitor::result_type account_name_type_visitor::operator()( const T& op )
{
  account_name_type_reflect_visitor aref{ op };

  fc::reflector<T>::visit( aref );

  return aref.get_impacted_accounts();
}

class accounts_from_operations_collector
{
public:
  using on_new_account_collected_t = std::function<void(const hp::account_name_type& )>;

private:
  account_name_type_visitor::result_type created_accounts;

  on_new_account_collected_t on_new_account_collected;

public:
  accounts_from_operations_collector( on_new_account_collected_t on_new_account_collected )
    : on_new_account_collected( on_new_account_collected )
  {}

  void collect( const hp::operation& op )
  {
    account_name_type_visitor atv{};

    for( const auto& acc : op.visit( atv ) )
      if(created_accounts.find(acc) == created_accounts.end())
      {
        created_accounts.emplace( acc );
        on_new_account_collected( acc );
      }
  }

};

} } } } }
