#pragma once

#include <core/beekeeper_wallet_base.hpp>

#include<vector>

namespace beekeeper {

typedef uint16_t transaction_handle_type;

struct wallet_data
{
   std::vector<char>              cipher_keys; /** encrypted keys */
};

namespace detail {
class beekeeper_impl;
}

/**
 * This wallet assumes it is connected to the database server with a high-bandwidth, low-latency connection and
 * performs minimal caching.
 */
class beekeeper_wallet final : public beekeeper_wallet_base
{
   public:
      beekeeper_wallet();

      ~beekeeper_wallet() override;

      /** Returns the current wallet filename.
       *
       * This is the filename that will be used when automatically saving the wallet.
       *
       * @see set_wallet_filename()
       * @return the wallet filename
       */
      string                            get_wallet_filename() const;

      /**
       * Get the WIF private key corresponding to a public key.  The
       * private key must already be in the wallet.
       */
      private_key_type get_private_key( public_key_type pubkey )const override;

      /**
       *  @param role - active | owner | posting | memo
       */
      pair<public_key_type,private_key_type>  get_private_key_from_password( string account, string role, string password )const;

      /** Checks whether the wallet has just been created and has not yet had a password set.
       *
       * Calling \c set_password will transition the wallet to the locked state.
       * @return true if the wallet is new
       * @ingroup Wallet Management
       */
      bool    is_new()const;

      /** Checks whether the wallet is locked (is unable to use its private keys).
       *
       * This state can be changed by calling \c lock() or \c unlock().
       * @return true if the wallet is locked
       * @ingroup Wallet Management
       */
      bool    is_locked()const override;

      /** Locks the wallet immediately.
       * @ingroup Wallet Management
       */
      void    lock() override;

      /** Unlocks the wallet.
       *
       * The wallet remain unlocked until the \c lock is called
       * or the program exits.
       * @param password the password previously set with \c set_password()
       * @ingroup Wallet Management
       */
      void    unlock(string password) override;

      /** Checks the password of the wallet
       *
       * Validates the password on a wallet even if the wallet is already unlocked,
       * throws if bad password given.
       * @param password the password previously set with \c set_password()
       * @ingroup Wallet Management
       */
      void    check_password(string password) override;

      /** Sets a new password on the wallet.
       *
       * The wallet must be either 'new' or 'unlocked' to
       * execute this command.
       * @ingroup Wallet Management
       */
      void    set_password(string password) override;

      /** Dumps all private keys owned by the wallet.
       *
       * The keys are printed in WIF format.  You can import these keys into another wallet
       * using \c import_key()
       * @returns a map containing the private keys, indexed by their public key
       */
      map<public_key_type, private_key_type> list_keys() override;

      /** Dumps all public keys owned by the wallet.
       * @returns a vector containing the public keys
       */
      flat_set<public_key_type> list_public_keys() override;

      /** Loads a specified Graphene wallet.
       *
       * The current wallet is closed before the new wallet is loaded.
       *
       * @warning This does not change the filename that will be used for future
       * wallet writes, so this may cause you to overwrite your original
       * wallet unless you also call \c set_wallet_filename()
       *
       * @param wallet_filename the filename of the wallet JSON file to load.
       *                        If \c wallet_filename is empty, it reloads the
       *                        existing wallet file
       * @returns true if the specified wallet is loaded
       */
      bool    load_wallet_file(string wallet_filename = "");

      /** Saves the current wallet to the given filename.
       *
       * @warning This does not change the wallet filename that will be used for future
       * writes, so think of this function as 'Save a Copy As...' instead of
       * 'Save As...'.  Use \c set_wallet_filename() to make the filename
       * persist.
       * @param wallet_filename the filename of the new wallet JSON file to create
       *                        or overwrite.  If \c wallet_filename is empty,
       *                        save to the current filename.
       */
      void    save_wallet_file(string wallet_filename = "");

      /** Sets the wallet filename used for future writes.
       *
       * This does not trigger a save, it only changes the default filename
       * that will be used the next time a save is triggered.
       *
       * @param wallet_filename the new filename to use for future saves
       */
      void    set_wallet_filename(string wallet_filename);

      /** Imports a WIF Private Key into the wallet to be used to sign transactions by an account.
       *
       * example: import_key 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3
       *
       * @param wif_key the WIF Private Key to import
       */
      string import_key( string wif_key ) override;

      /** Removes a key from the wallet.
       *
       * example: remove_key 6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
       *
       * @param key the Public Key to remove
       */
      bool remove_key( string key ) override;

      /* Attempts to sign a digest via the given public_key
      */
      std::optional<signature_type> try_sign_digest( const digest_type& sig_digest, const public_key_type& public_key ) override;

      std::shared_ptr<detail::beekeeper_impl> my;
      void encrypt_keys();

      /** Tests if a private key corresponding to a public key exists in a wallet
       *
       * example: has_matching_private_key 6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
       *
       * @param key the Public Key to remove
       */
      bool has_matching_private_key( const public_key_type& public_key ) override;
};

struct plain_keys {
   fc::sha512                            checksum;
   map<public_key_type,private_key_type> keys;
};

} //beekeeper_wallet

namespace fc
{
  void from_variant( const fc::variant& var, beekeeper::wallet_data& vo );
  void to_variant( const beekeeper::wallet_data& var, fc::variant& vo );
}

FC_REFLECT( beekeeper::wallet_data, (cipher_keys) )

FC_REFLECT( beekeeper::plain_keys, (checksum)(keys) )
