#pragma once

#include <hive/protocol/authority.hpp>
#include <hive/protocol/config.hpp>
#include <hive/protocol/sign_state_types.hpp>
#include <hive/protocol/types.hpp>

namespace hive { namespace protocol {

struct sign_limits
{
  bool allow_strict_and_mixed_authorities = false;
  uint32_t recursion = HIVE_MAX_SIG_CHECK_DEPTH;
  uint32_t membership = ~0;
  uint32_t account_auths = ~0;
};

class sign_state
{
    size_t account_auth_count       = 0;

    authority_getter                get_current_authority;

    const sign_limits               limits;
    flat_set<string>                approved_by;
    flat_map<public_key_type,bool>  provided_signatures;

    bool check_authority_impl( const authority& auth, uint32_t depth )
    {
      uint32_t total_weight = 0;

      size_t membership = 0;

      auto _increase_membership = [&membership, this]()
      {
        ++membership;
        if( limits.membership > 0 && membership >= limits.membership )
          return false;
        return true;
      };

      for( const auto& k : auth.key_auths )
      {
        if( signed_by( k.first ) )
        {
          total_weight += k.second;
          if( total_weight >= auth.weight_threshold )
            return true;
        }

        if( !_increase_membership() )
          return false;
      }

      for( const auto& a : auth.account_auths )
      {
        if( approved_by.find(a.first) == approved_by.end() )
        {
          if( depth == limits.recursion )
            continue;

          if( limits.account_auths > 0 && account_auth_count >= limits.account_auths )
          {
            return false;
          }

          ++account_auth_count;

          if( check_authority_impl( get_current_authority( a.first ), depth + 1 ) )
          {
            approved_by.insert( a.first );
            total_weight += a.second;
            if( total_weight >= auth.weight_threshold )
              return true;
          }
        }
        else
        {
          total_weight += a.second;
          if( total_weight >= auth.weight_threshold )
            return true;
        }

        if( !_increase_membership() )
          return false;
      }
      return total_weight >= auth.weight_threshold;
    }

  public:

    sign_state( const flat_set<public_key_type>& sigs, const authority_getter& a, const sign_limits& limits )
      : get_current_authority( a ), limits( limits )
    {
      extend_provided_signatures( sigs );
      init_approved();
    }

    /** returns true if we have a signature for this key or can
      * produce a signature for this key, else returns false.
      */
    bool signed_by( const public_key_type& k )
    {
      auto itr = provided_signatures.find(k);
      if( itr == provided_signatures.end() )
        return false;
      else
        itr->second = true;
      return true;
    }

    bool check_authority( const string& id )
    {
      if( approved_by.find(id) != approved_by.end() ) return true;

      if( limits.allow_strict_and_mixed_authorities )
        ++account_auth_count;
      else
        account_auth_count = 1;

      return check_authority_impl( get_current_authority(id), 0 );
    }

    /**
      *  Checks to see if we have signatures of the active authorites of
      *  the accounts specified in authority or the keys specified.
      */
    bool check_authority( const authority& auth )
    {
      if( !limits.allow_strict_and_mixed_authorities )
        account_auth_count = 0;

      return check_authority_impl( auth, 0 );
    }

    bool remove_unused_signatures()
    {
      vector<public_key_type> remove_sigs;
      for( const auto& sig : provided_signatures )
        if( !sig.second ) remove_sigs.push_back( sig.first );

      for( auto& sig : remove_sigs )
        provided_signatures.erase(sig);

      return remove_sigs.size() != 0;
    }

    void init_approved()
    {
      approved_by.clear();
      approved_by.insert( "temp" );
    }

    void add_approved( const flat_set<account_name_type>& approvals )
    {
      std::copy( approvals.begin(), approvals.end(), std::inserter( approved_by, approved_by.end() ) );
    }

    void extend_provided_signatures( const flat_set<public_key_type>& keys )
    {
      for( const auto& key : keys )
        provided_signatures[ key ] = false;
    }

    const flat_map<public_key_type,bool>&  get_provided_signatures() const { return provided_signatures; }

    void change_current_authority( const authority_getter& a )
    {
      get_current_authority = a;
    }
};

} } // hive::protocol
