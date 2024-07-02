#pragma once

#include <core/utilities.hpp>

using namespace std;

namespace beekeeper {

class beekeeper_wallet_base
{
   public:
      virtual ~beekeeper_wallet_base() {}

      /**
       * Get the private key corresponding to a public key.  The
       * private key must already be in the wallet.
       */
      virtual private_key_type get_private_key( public_key_type pubkey ) const = 0;

      /** Checks whether the wallet is locked (is unable to use its private keys).
       *
       * This state can be changed by calling \c lock() or \c unlock().
       * @return true if the wallet is locked
       * @ingroup Wallet Management
       */
      virtual bool    is_locked() const = 0;

      /** Locks the wallet immediately
       * @ingroup Wallet Management
       */
      virtual void    lock() = 0;

      /** Unlocks the wallet.
       *
       * The wallet remain unlocked until the \c lock is called
       * or the program exits.
       * @param password the password previously set with \c set_password()
       * @ingroup Wallet Management
       */
      virtual void    unlock(string password) = 0;

      /** Checks the password of the wallet
       *
       * Validates the password on a wallet even if the wallet is already unlocked,
       * throws if bad password given.
       * @param password the password previously set with \c set_password()
       * @ingroup Wallet Management
       */
      virtual void    check_password(string password) = 0;

      /** Sets a new password on the wallet.
       *
       * The wallet must be either 'new' or 'unlocked' to
       * execute this command.
       * @ingroup Wallet Management
       */
      virtual void    set_password(string password) = 0;

      /** Dumps all private keys owned by the wallet.
       *
       * The keys are printed in WIF format.  You can import these keys into another wallet
       * using \c import_key()
       * @returns a map containing the private keys, indexed by their public key
       */
      virtual keys_details list_keys() = 0;

      /** Dumps all public keys owned by the wallet.
       * @returns a vector containing the public keys
       */
      virtual keys_details list_public_keys() = 0;

      /** Imports a WIF Private Key into the wallet to be used to sign transactions by an account.
       *
       * @param wif_key the WIF Private Key to import
       * @param prefix a prefix of a public key
       */
      virtual string import_key( const string& wif_key, const string& prefix ) = 0;

      /** Imports a WIF Private Keys into the wallet to be used to sign transactions by an account.
       *
       * @param wif_keys the WIF Private Keys to import
       * @param prefix a prefix of a public key
       */
      virtual std::vector<std::string> import_keys( const std::vector<string>& wif_keys, const string& prefix ) = 0;

      /** Removes a key from the wallet.
       *
       * @param key the Public Key to remove
       */
      virtual bool remove_key( const public_key_type& public_key ) = 0;

      /** Returns a signature given the digest and public_key, if this wallet can sign via that public key
       */
      virtual std::optional<signature_type> try_sign_digest( const digest_type& sig_digest, const public_key_type& public_key ) = 0;

      /** Tests if a private key corresponding to a public key exists in a wallet
       *
       * @param a public key to test
       * @returns informatio if a private key exists
       */
      virtual bool has_matching_private_key( const public_key_type& public_key ) = 0;
};

}
