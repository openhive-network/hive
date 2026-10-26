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

/**
 * Unpacks value returned by authority_getter_i, throws when given account was not found.
 */
template< typename T >
const T& force_found( const std::optional<T>& value, const string& id )
{
  FC_ASSERT( value.has_value(), "Account ${id} not found", ( id ) );
  return *value;
}

template <bool IS_TRACED=false>
class sign_state
{
    size_t account_auth_count       = 0;

    const authority_getter_i&       getters;
    authority_getter_method         get_current_authority;

    const sign_limits               limits;
    flat_set<string>                approved_by;
    flat_map<public_key_type,bool>  provided_signatures;
    authority_verification_tracer*  tracer = nullptr;

  public:

    /**
     * @param getters source of authorities; unknown accounts are reported to tracer when IS_TRACED, otherwise exception is thrown
     * @param current_authority selects role of authorities used by check_authority( id ) and for account entries
     * @param the_tracer mandatory when IS_TRACED, ignored otherwise
     */
    sign_state( const flat_set<public_key_type>& sigs,
      const authority_getter_i& getters,
      authority_getter_method current_authority,
      const sign_limits& limits,
      authority_verification_tracer* the_tracer = nullptr )
      : getters( getters ), get_current_authority( current_authority ), limits( limits ), tracer(the_tracer)
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
      std::optional<authority> initial_auth = ( getters.*get_current_authority )( id );
      if constexpr (IS_TRACED)
      {
        FC_ASSERT( tracer && "check_authority 1", "check_authority: tracer is null", ( "id", id ) );
        if( !initial_auth )
        {
          tracer->on_unknown_account_entry( id, 0, 0, 0 );
          return false;
        }

        tracer->on_root_authority_start( id, initial_auth->weight_threshold, 0 );
      }
      else
      {
        force_found( initial_auth, id );
      }

      if( approved_by.find(id) != approved_by.end() )
      {
        if constexpr (IS_TRACED)
        {
          FC_ASSERT( tracer && "check_authority 2", "check_authority: tracer is null", ( "id", id ) );
          tracer->on_approved_authority( id, initial_auth->weight_threshold );
        }

        return true;
      }

      if( limits.allow_strict_and_mixed_authorities )
        ++account_auth_count;
      else
        account_auth_count = 1;

      bool success = check_authority_impl( *initial_auth, 0 );

      if constexpr (IS_TRACED)
      {
        FC_ASSERT( tracer && "check_authority 3", "check_authority: tracer is null", ( "id", id ) );
        // TODO: Provide appropriate set of flags.
        tracer->on_root_authority_finish( success, 0 );
      }

      return success;
    }

    /**
      *  Checks to see if we have signatures of the active authorites of
      *  the accounts specified in authority or the keys specified.
      */
    bool check_authority( const authority& auth, const string& id, const string& role )
    {
      if constexpr (IS_TRACED)
      {
        FC_ASSERT( tracer && "check_authority 4", "check_authority: tracer is null", ( "id", id ) );
        tracer->set_role( role );
        tracer->on_root_authority_start( id, auth.weight_threshold, 0 );
      }

      if( !limits.allow_strict_and_mixed_authorities )
        account_auth_count = 0;

      bool success = check_authority_impl( auth, 0 );

      if constexpr (IS_TRACED)
      {
        FC_ASSERT( tracer && "check_authority 5", "check_authority: tracer is null", ( "id", id ) );
        // TODO: Provide appropriate set of flags.
        tracer->on_root_authority_finish( success, 0 );
      }

      return success;
    }

    /**
      *  Checks authority of given account in the role selected by getter (regardless of current authority role).
      *  Throws when the account is not known.
      */
    bool check_authority( authority_getter_method getter, const string& id, const string& role )
    {
      return check_authority( force_found( ( getters.*getter )( id ), id ), id, role );
    }

    bool remove_unused_signatures()
    {
      vector<public_key_type> remove_sigs;
      for( const auto& sig : provided_signatures )
        if( !sig.second )
          remove_sigs.push_back( sig.first );

      for( auto& sig : remove_sigs )
        provided_signatures.erase( sig );

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

    const flat_map< public_key_type, bool >& get_provided_signatures() const { return provided_signatures; }

    void change_current_authority( authority_getter_method a )
    {
      get_current_authority = a;
    }

  private:

    bool check_authority_impl( const authority& auth, uint32_t depth )
    {
      uint32_t total_weight = 0;

      size_t membership = 0;

      if constexpr (IS_TRACED)
      {
        FC_ASSERT( tracer && "check_authority 6" );
        if( auth.key_auths.empty() && auth.account_auths.empty() )
          tracer->on_empty_auth();
      }

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

          if constexpr (IS_TRACED)
          {
            FC_ASSERT( tracer && "check_authority 7" );
            tracer->on_matching_key( k.first, k.second, auth.weight_threshold, depth,
                                     total_weight >= auth.weight_threshold );
          }

          if( total_weight >= auth.weight_threshold )
            return true;
        }

        if( !_increase_membership() )
          return false;
      }

      if constexpr (IS_TRACED)
      {
        FC_ASSERT( tracer && "check_authority 8" );
        tracer->on_missing_matching_key();
      }

      for( const auto& a : auth.account_auths )
      {
        if( approved_by.find(a.first) == approved_by.end() )
        {
          if( depth == limits.recursion )
          {
            if constexpr (IS_TRACED)
            {
              FC_ASSERT( tracer && "check_authority 9" );
              tracer->on_recursion_depth_limit_exceeded();
            }

            continue;
          }

          if( limits.account_auths > 0 && account_auth_count >= limits.account_auths )
          {
            if constexpr (IS_TRACED)
            {
              FC_ASSERT( tracer && "check_authority 10" );
              tracer->on_account_processing_limit_exceeded();
            }

            return false;
          }

          ++account_auth_count;

          std::optional< authority > account_auth = ( getters.*get_current_authority )( a.first );
          if constexpr (IS_TRACED)
          {
            FC_ASSERT( tracer && "check_authority 11" );
            if( !account_auth )
            {
              tracer->on_unknown_account_entry( a.first, a.second, 0 /*unknown too*/, depth );
              continue;
            }

            tracer->on_entering_account_entry( a.first, a.second, account_auth->weight_threshold, depth );
          }
          else
          {
            force_found( account_auth, a.first );
          }

          bool success = check_authority_impl( *account_auth, depth + 1 );
          if( success )
          {
            approved_by.insert( a.first );
            total_weight += a.second;
            if( total_weight >= auth.weight_threshold )
            {
              if constexpr (IS_TRACED)
              {
                FC_ASSERT( tracer && "check_authority 12" );
                tracer->on_leaving_account_entry( a.second, true /*parent_threshold_reached*/ );
              }
              return true;
            }
          }
          if constexpr (IS_TRACED)
          {
            FC_ASSERT( tracer && "check_authority 13" );
            tracer->on_leaving_account_entry( success ? a.second : 0, false /*parent_threshold_reached*/ );
          }
        }
        else
        {
          if constexpr (IS_TRACED)
          {
            FC_ASSERT( tracer && "check_authority 14" );
            tracer->on_approved_authority( a.first, a.second );
          }

          total_weight += a.second;
          if( total_weight >= auth.weight_threshold )
            return true;
        }

        if( !_increase_membership() )
          return false;
      }
      return total_weight >= auth.weight_threshold;
    }
};

} } // hive::protocol
