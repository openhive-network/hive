#pragma once

#include <hive/protocol/authority.hpp>
#include <hive/protocol/authority_verification_tracer.hpp>
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

template <bool IS_TRACED=false>
class sign_state
{
    size_t account_auth_count       = 0;

    authority_getter                get_current_authority;

    const sign_limits               limits;
    flat_set<string>                approved_by;
    flat_map<public_key_type,bool>  provided_signatures;
    authority_verification_tracer*  tracer = nullptr;

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
          if constexpr (IS_TRACED) {
            FC_ASSERT(tracer);
            tracer->on_matching_key(k.first, k.second, auth.weight_threshold, depth);
          }

          total_weight += k.second;
          if( total_weight >= auth.weight_threshold )
            return true;
        }

        if( !_increase_membership() )
          return false;
      }

      if constexpr (IS_TRACED) {
        FC_ASSERT(tracer);
        tracer->on_missing_matching_key();
      }

      for( const auto& a : auth.account_auths )
      {
        if( approved_by.find(a.first) == approved_by.end() )
        {
          if( depth == limits.recursion )
          {
            if constexpr (IS_TRACED) {
              FC_ASSERT(tracer);
              tracer->on_recursion_depth_limit_exceeded();
            }

            continue;
          }

          if( limits.account_auths > 0 && account_auth_count >= limits.account_auths )
          {
            if constexpr (IS_TRACED) {
              FC_ASSERT(tracer);
              tracer->on_account_processing_limit_exceeded();
            }

            return false;
          }

          ++account_auth_count;

          authority account_auth;
          if constexpr (IS_TRACED) {
            FC_ASSERT(tracer);
            try
            {
              account_auth = get_current_authority( a.first );
            } catch( const std::runtime_error& e )
            {
              // TODO: Call on_unknown_account_entry here
              continue;
            }           

            tracer->on_entering_account_entry( a.first, a.second, auth.weight_threshold, depth );
          }
          else
            account_auth = get_current_authority( a.first );

          if( check_authority_impl( account_auth, depth + 1 ) )
          {
            approved_by.insert( a.first );
            total_weight += a.second;
            if( total_weight >= auth.weight_threshold )
            {
              if constexpr (IS_TRACED) {
                FC_ASSERT(tracer);
                tracer->on_leaving_account_entry( true );
              }
              return true;
            }
          }
          if constexpr (IS_TRACED) {
            FC_ASSERT(tracer);
            tracer->on_leaving_account_entry( a == *(auth.account_auths.crbegin()) );
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

    sign_state( const flat_set<public_key_type>& sigs,
      const authority_getter& a,
      const sign_limits& limits,
      authority_verification_tracer* the_tracer = nullptr )
      : get_current_authority( a ), limits( limits ), tracer(the_tracer)
    {
      extend_provided_signatures( sigs );
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
      authority initial_auth;
      if constexpr (IS_TRACED) {
        FC_ASSERT(tracer);
        try
        {
          initial_auth = get_current_authority( id );
        } catch( const std::runtime_error& e )
        {
          // TODO: Call on_unknown_account_entry here
          return false;
        }           

        tracer->on_root_authority_start(id, initial_auth.weight_threshold, 0);
      }
      else
        initial_auth = get_current_authority( id );

      // TODO Determine whether any trace of approved_by is needed.
      if( approved_by.find(id) != approved_by.end() ) return true;

      if( limits.allow_strict_and_mixed_authorities )
        ++account_auth_count;
      else
        account_auth_count = 1;

      bool success = check_authority_impl( initial_auth, 0 );

      if constexpr (IS_TRACED) {
          FC_ASSERT(tracer);
          // TODO: Provide appropriate set of flags.
          tracer->on_root_authority_finish(0);
      }

      return success;
    }

    /**
      *  Checks to see if we have signatures of the active authorites of
      *  the accounts specified in authority or the keys specified.
      */
    bool check_authority( const authority& auth, const string& id, const string& role )
    {
      if constexpr (IS_TRACED) {
          FC_ASSERT(tracer);
          tracer->set_role(role);
          tracer->on_root_authority_start(id, auth.weight_threshold, 0);
      }

      if( !limits.allow_strict_and_mixed_authorities )
        account_auth_count = 0;

      bool success = check_authority_impl( auth, 0 );

      if constexpr (IS_TRACED) {
          FC_ASSERT(tracer);
          // TODO: Provide appropriate set of flags.
          tracer->on_root_authority_finish(0);
      }

      return success;
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

    void clear_approved()
    {
      approved_by.clear();
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
