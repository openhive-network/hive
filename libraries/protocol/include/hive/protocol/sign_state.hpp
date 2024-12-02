#pragma once

#include <hive/protocol/authority.hpp>
#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>

namespace hive { namespace protocol {

typedef std::function<authority(const string&)> authority_getter;
typedef std::function<public_key_type(const string&)> witness_public_key_getter;

struct sign_limits
{
  uint32_t recursion = HIVE_MAX_SIG_CHECK_DEPTH;
  uint32_t membership = ~0;
  uint32_t account_auths = ~0;
};

class sign_state
{
    bool allow_mixed_authorities    = false;
    size_t account_auth_count       = 0;

    authority_getter                get_current_authority;

    const sign_limits               limits;
    flat_set<string>                approved_by;
    flat_map<public_key_type,bool>  provided_signatures;

    bool check_authority_impl( const authority& au, uint32_t depth );

  public:

    sign_state( bool allow_mixed_authorities, const flat_set<public_key_type>& sigs, const authority_getter& a, const sign_limits& limits );

    /** returns true if we have a signature for this key or can
      * produce a signature for this key, else returns false.
      */
    bool signed_by( const public_key_type& k );
    bool check_authority( const string& id );

    /**
      *  Checks to see if we have signatures of the active authorites of
      *  the accounts specified in authority or the keys specified.
      */
    bool check_authority( const authority& au, uint32_t depth = 0 );

    bool remove_unused_signatures();

    void init_approved();
    void add_approved( const flat_set<account_name_type>& approvals );

    void extend_provided_signatures( const flat_set<public_key_type>& keys );
    const flat_map<public_key_type,bool>&  get_provided_signatures() const { return provided_signatures; }

    void change_current_authority( const authority_getter& a );
};

} } // hive::protocol
