#include <fc/exception/exception.hpp>

namespace hive { namespace plugins { namespace clive {
  FC_DECLARE_EXCEPTION( clive_exception, 3000000, "clive exception" )

  FC_DECLARE_DERIVED_EXCEPTION( clive_exist_exception,            clive_exception,
                                3120001, "clive already exists" )
  FC_DECLARE_DERIVED_EXCEPTION( clive_nonexistent_exception,      clive_exception,
                                3120002, "Nonexistent clive" )
  FC_DECLARE_DERIVED_EXCEPTION( clive_locked_exception,           clive_exception,
                                3120003, "Locked clive" )
  FC_DECLARE_DERIVED_EXCEPTION( clive_missing_pub_key_exception,  clive_exception,
                                3120004, "Missing public key" )
  FC_DECLARE_DERIVED_EXCEPTION( clive_invalid_password_exception, clive_exception,
                                3120005, "Invalid clive password" )
  FC_DECLARE_DERIVED_EXCEPTION( clive_not_available_exception,    clive_exception,
                                3120006, "No available clive" )
  FC_DECLARE_DERIVED_EXCEPTION( clive_unlocked_exception,         clive_exception,
                                3120007, "Already unlocked" )
  FC_DECLARE_DERIVED_EXCEPTION( key_exist_exception,               clive_exception,
                                3120008, "Key already exists" )
  FC_DECLARE_DERIVED_EXCEPTION( key_nonexistent_exception,         clive_exception,
                                3120009, "Nonexistent key" )
  FC_DECLARE_DERIVED_EXCEPTION( unsupported_key_type_exception,    clive_exception,
                                3120010, "Unsupported key type" )
  FC_DECLARE_DERIVED_EXCEPTION( invalid_lock_timeout_exception,    clive_exception,
                                3120011, "clive lock timeout is invalid" )
} } }