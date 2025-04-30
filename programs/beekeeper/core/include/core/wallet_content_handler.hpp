#pragma once

#include <core/utilities.hpp>

#include<vector>
#include<string>

namespace beekeeper {

typedef uint16_t transaction_handle_type;

namespace detail {
class beekeeper_impl;
}

/**
 * This wallet assumes it is connected to the database server with a high-bandwidth, low-latency connection and
 * performs minimal caching.
 */
class wallet_content_handler
{
   public:

      using ptr = std::shared_ptr<wallet_content_handler>;

   private:

      bool is_temporary = false;

      bool is_checksum_valid( const fc::sha512& old_checksum, const std::vector<char>& content );

      std::vector<char> get_decrypted_password( const std::string& password );

   public:
      wallet_content_handler( bool is_temporary = false );

      ~wallet_content_handler();

      bool is_wallet_temporary() const { return is_temporary; };

      /** Returns the current wallet filename.
       *
       * This is the filename that will be used when automatically saving the wallet.
       *
       * @return if a wallet is temporary returns a wallet name otherwise a wallet file name
       */
      const std::string& get_wallet_name() const;

      /**
       * Get the WIF private key corresponding to a public key.  The
       * private key must already be in the wallet.
       */
      private_key_type get_private_key( public_key_type pubkey ) const;

      /**
       *  @param role - active | owner | posting | memo
       */
      std::pair<public_key_type,private_key_type>  get_private_key_from_password( std::string account, std::string role, std::string password )const;

      /** Checks whether the wallet is locked (is unable to use its private keys).
       *
       * This state can be changed by calling \c lock() or \c unlock().
       * @return true if the wallet is locked
       * @ingroup Wallet Management
       */
      bool    is_locked()const;

      /** Locks the wallet immediately.
       * @ingroup Wallet Management
       */
      void    lock();

      /** Unlocks the wallet.
       *
       * The wallet remain unlocked until the \c lock is called
       * or the program exits.
       * @param password the password previously set with \c set_password()
       * @ingroup Wallet Management
       */
      void    unlock( const std::string& password );

      /** Checks the password of the wallet
       *
       * Validates the password on a wallet even if the wallet is already unlocked,
       * throws if bad password given.
       * @param password the password previously set with \c set_password()
       * @ingroup Wallet Management
       */
      void    check_password( const std::string& password );

      /** Sets a new password on the wallet.
       *
       * The wallet must be either 'new' or 'unlocked' to
       * execute this command.
       * @ingroup Wallet Management
       */
      void    set_password( const std::string& password );

      /** Returns details about private/public keys. In further processing proper data is extracted i.e.
       * for `get_public_keys` endpoint only public keys are returned, but for saving locally data for backup are returned both keys (private/public).
       * @returns a std::vector containing the public keys
       */
      keys_details get_keys_details();

      /** Loads a specified Graphene wallet.
       *
       * The current wallet is closed before the new wallet is loaded.
       *
       * @warning This does not change the filename that will be used for future
       * wallet writes, so this may cause you to overwrite your original
       * wallet unless you also call \c set_wallet_name()
       *
       * @param wallet_filename the filename of the wallet JSON file to load.
       *                        If \c wallet_filename is empty, it reloads the
       *                        existing wallet file
       * @returns true if the specified wallet is loaded
       */
      bool    load_wallet_file( const std::string& wallet_filename = "" );

      /** Sets the wallet filename used for future writes.
       *
       * This does not trigger a save, it only changes the default filename
       * that will be used the next time a save is triggered.
       *
       * @param wallet_filename the new filename to use for future saves
       */
      void    set_wallet_name( const std::string& wallet_name );

      /** Saves the current wallet to the given filename.
       *
       * @warning This does not change the wallet filename that will be used for future
       * writes, so think of this function as 'Save a Copy As...' instead of
       * 'Save As...'.  Use \c set_wallet_name() to make the filename
       * persist.
       * @param wallet_filename the filename of the new wallet JSON file to create
       *                        or overwrite.  If \c wallet_filename is empty,
       *                        save to the current filename.
       */
      void    save_wallet_file( const std::string& wallet_filename = "" );

      /** Imports a WIF Private Key into the wallet to be used to sign transactions by an account.
       *
       * @param wif_key the WIF Private Key to import
       * @param prefix a prefix of a public key
       */
      std::string import_key( const std::string& wif_key, const std::string& prefix );

      /** Imports a WIF Private Keys into the wallet to be used to sign transactions by an account.
       *
       * @param wif_key the WIF Private Keys to import
       * @param prefix a prefix of a public key
       */
      std::vector<std::string> import_keys( const std::vector<std::string>& wif_keys, const std::string& prefix );

      /** Removes a key from the wallet.
       *
       * @param key the Public Key to remove
       */
      bool remove_key( const public_key_type& public_key );

      /* Attempts to sign a digest via the given public_key
      */
      std::optional<signature_type> try_sign_digest( const digest_type& sig_digest, const public_key_type& public_key );

      std::shared_ptr<detail::beekeeper_impl> my;
      void encrypt_keys();

      /** Tests if a private key corresponding to a public key exists in a wallet
       *
       * @param key the Public Key to remove
       */
      bool has_matching_private_key( const public_key_type& public_key );
};

} //wallet_content_handler
