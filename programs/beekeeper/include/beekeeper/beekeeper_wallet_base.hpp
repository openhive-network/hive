#pragma once


#include <fc/container/flat_fwd.hpp>
#include <hive/protocol/config.hpp>

using namespace std;

namespace beekeeper {

using private_key_type  = fc::ecc::private_key;
using public_key_type   = fc::ecc::public_key;
using signature_type    = fc::ecc::compact_signature;
using digest_type       = fc::sha256;
using chain_id_type     = hive::protocol::chain_id_type;

using fc::flat_set;

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
      virtual map<public_key_type, private_key_type> list_keys() = 0;

      /** Dumps all public keys owned by the wallet.
       * @returns a vector containing the public keys
       */
      virtual flat_set<public_key_type> list_public_keys() = 0;

      /** Imports a WIF Private Key into the wallet to be used to sign transactions by an account.
       *
       * example: import_key 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3
       *
       * @param wif_key the WIF Private Key to import
       */
      virtual string import_key( string wif_key ) = 0;

      /** Removes a key from the wallet.
       *
       * example: remove_key EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
       *
       * @param key the Public Key to remove
       */
      virtual bool remove_key( string key ) = 0;

      /** Returns a signature given the digest and public_key, if this wallet can sign via that public key
       */
      virtual std::optional<signature_type> try_sign_digest( const public_key_type& public_key, const digest_type& sig_digest ) = 0;

      /** Returns a signature using a serialized transaction, a chain id and a public key. A parameter sig_digest is used for verification.
       */
      virtual std::optional<signature_type> try_sign_transaction( const string& transaction, const chain_id_type& chain_id, const public_key_type& public_key, const digest_type& sig_digest ) = 0;
};

}
