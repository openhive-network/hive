#include <fc/exception/exception.hpp>

namespace hive { namespace plugins { namespace wallet {
  FC_DECLARE_EXCEPTION( wallet_exception, 3000000, "Wallet exception" )

  FC_DECLARE_DERIVED_EXCEPTION( wallet_exist_exception,            wallet_exception,
                                3120001, "Wallet already exists" )
  FC_DECLARE_DERIVED_EXCEPTION( wallet_nonexistent_exception,      wallet_exception,
                                3120002, "Nonexistent wallet" )
  FC_DECLARE_DERIVED_EXCEPTION( wallet_locked_exception,           wallet_exception,
                                3120003, "Locked wallet" )
  FC_DECLARE_DERIVED_EXCEPTION( wallet_missing_pub_key_exception,  wallet_exception,
                                3120004, "Missing public key" )
  FC_DECLARE_DERIVED_EXCEPTION( wallet_invalid_password_exception, wallet_exception,
                                3120005, "Invalid wallet password" )
  FC_DECLARE_DERIVED_EXCEPTION( wallet_not_available_exception,    wallet_exception,
                                3120006, "No available wallet" )
  FC_DECLARE_DERIVED_EXCEPTION( wallet_unlocked_exception,         wallet_exception,
                                3120007, "Already unlocked" )
  FC_DECLARE_DERIVED_EXCEPTION( key_exist_exception,               wallet_exception,
                                3120008, "Key already exists" )
  FC_DECLARE_DERIVED_EXCEPTION( key_nonexistent_exception,         wallet_exception,
                                3120009, "Nonexistent key" )
  FC_DECLARE_DERIVED_EXCEPTION( unsupported_key_type_exception,    wallet_exception,
                                3120010, "Unsupported key type" )
  FC_DECLARE_DERIVED_EXCEPTION( invalid_lock_timeout_exception,    wallet_exception,
                                3120011, "Wallet lock timeout is invalid" )
} } }