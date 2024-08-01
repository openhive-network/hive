#pragma once

#include <core/beekeeper_instance_base.hpp>
#include <core/session_manager_base.hpp>

#include<csignal>

namespace beekeeper {

  /**
   *
   * Provides associate of wallet name to a wallet and manages the interaction with each wallet.

   * The name of the wallet is also used as part of the file name. See beekeeper_wallet_manager::create.
   * No const methods because timeout may cause lock_all() to be called.
   */
class beekeeper_wallet_manager
{
private:

  using close_all_sessions_action_method = std::function<void()>;

  void set_timeout_impl( const std::string& token, seconds_type seconds );

public:

  /**
   *
   * @param sessions           Sessions' manager.
   * @param instance           An instance of beekeeper service.
   * @param cmd_wallet_dir     Path to a location of wallet files. By default './'.
   * @param cmd_unlock_timeout Timeout for an unlocked wallet in seconds. By default 900[s].
   * @param cmd_session_limit  Number of concurrent sessions. By default 64.
   * @param method             An action that is executed when all sessions are closed.
   */
  beekeeper_wallet_manager( std::shared_ptr<session_manager_base> sessions, std::shared_ptr<beekeeper_instance_base> instance,
                            const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit,
                            close_all_sessions_action_method&& method = [](){} );

  beekeeper_wallet_manager(const beekeeper_wallet_manager&) = delete;
  beekeeper_wallet_manager(beekeeper_wallet_manager&&) = delete;
  beekeeper_wallet_manager& operator=(const beekeeper_wallet_manager&) = delete;
  beekeeper_wallet_manager& operator=(beekeeper_wallet_manager&&) = delete;

  ~beekeeper_wallet_manager(){}

  bool start();

  /**
   * 
   * Set a timeout in order to lock all wallets when time pass.
   * After `N` seconds of inactivity `lock_all` is called.
   * @param token    Session's identifier.
   * @param seconds  A timeout in seconds.
   */
  void set_timeout( const std::string& token, seconds_type seconds );

  /**
   * 
   * Sign a sig_digest using a private key corresponding to a public key.
   * @param token       Session's identifier.
   * @param wallet_name A name of a wallet where a private key is stored. Optional. If not given, then a private key is searched in all unlocked wallets.
   * @param sig_digest  A signature digest. Represents a whole transaction.
   * @param public_key  A public key corresponding to a private key that is stored in a wallet.
   * @param prefix      A prefix of a public key
   * @return            A signature of a transaction.
   * @throws            An exception `fc::exception` if a corresponding private key is not found in unlocked wallet/wallets.
   */
  signature_type sign_digest( const std::string& token, const std::optional<std::string>& wallet_name, const std::string& sig_digest, const public_key_type& public_key, const std::string& prefix );

  /**
   *
   * Create a new wallet.
   * A new wallet is created in file dir/{wallet_name}.wallet. The new wallet is unlocked after creation.
   * @param token       Session's identifier.
   * @param wallet_name A name of a wallet.
   * @param password    A password for a wallet, if not given will be automatically generated.
   * @param is_temporary  If `true` the wallet exists only in memory otherwise is saved into a file.
   * @return            A plaintext password that is needed to unlock a wallet. A caller is responsible for saving the password otherwise it's impossible to unlock the wallet.
   * @throws            An exception `fc`::exception if wallet with name already exists (or filename already exists).
   */
  std::string create( const std::string& token, const std::string& wallet_name, const std::optional<std::string>& password, const std::optional<bool>& is_temporary );

  /**
   *
   * Open an existing wallet. Opening does not unlock the wallet.
   * @param token       Session's identifier.
   * @param wallet_name A name of a wallet.
   * @throws            An exception `fc::exception` if it's unable to find/open the wallet file.
   */
  void open( const std::string& token, const std::string& wallet_name );

  /**
   *
   * Close an existing wallet. It locks the wallet.
   * @param token       Session's identifier.
   * @param wallet_name A name of a wallet.
   * @throws            An exception `fc::exception` if unable to find/close the wallet file.
   */
  void close( const std::string& token, const std::string& wallet_name );

  /**
   *
   * List all opened wallets.
   * @param token Session's identifier.
   * @return      A list of wallets with information if given wallet is unlocked.
   */
  std::vector<wallet_details> list_wallets( const std::string& token );

  /**
   * 
   * List all created wallets
   * @param token Session's identifier.
   * @return      A list of created wallets. Size of vector equals to a number of created files.
   */
  std::vector<wallet_details> list_created_wallets( const std::string& token );

  /**
   * 
   * @param token       Session's identifier.
   * @param wallet_name A name of a wallet.
   * @param password    A password of given wallet.
   * @return            A list of private keys.
   */
  keys_details list_keys( const std::string& token, const std::string& wallet_name, const std::string& password );

  /**
   *
   * List all public keys from one wallet if a `wallet_name` is given otherwise all public keys from all unlocked wallets.
   * @param token       Session's identifier.
   * @param wallet_name A name of a wallet.
   * @return            A set of public keys from all unlocked wallets
   */
  keys_details get_public_keys( const std::string& token, const std::optional<std::string>& wallet_name );

  /**
   * 
   * Locks all the unlocked wallets.
   * @param token Session's identifier.
   */
  void lock_all( const std::string& token );

  /**
   * 
   * Lock specified wallet.
   * @param token       Session's identifier.
   * @param wallet_name A name of a wallet to lock.
   * @throws            A exception `fc::exception` if `wallet_name` not found.
   */
  void lock( const std::string& token, const std::string& wallet_name );

  /**
   * 
   * Unlock specified wallet.
   * @param token       Session's identifier.
   * @param wallet_name A name of a wallet to unlock.
   * @param password    The plaintext password.
   * @throws            An exception `fc::exception` if wallet not found or invalid password or already unlocked.
   */
  void unlock( const std::string& token, const std::string& wallet_name, const std::string& password );


  /**
   * 
   * Import a private key into specified wallet.
   * Wallet must be opened and unlocked.
   * @param token       Session's identifier.
   * @param wallet_name A name of a wallet to import into.
   * @param wif_key     The WIF Private Key to import, e.g. 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n.
   * @param prefix      A prefix of a public key
   * @return            A public key corresponding to WIF key
   * @throws            An exception `fc::exception` if a wallet not found or locked.
   */
  std::string import_key( const std::string& token, const std::string& wallet_name, const std::string& wif_key, const std::string& prefix );

  /**
   * 
   * Import a private key into specified wallet.
   * Wallet must be opened and unlocked.
   * @param token       Session's identifier.
   * @param wallet_name A name of a wallet to import into.
   * @param wif_keys    The WIF Private Keys to import, e.g. 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n.
   * @param prefix      A prefix of a public key
   * @return            A public keys corresponding to WIF keys
   * @throws            An exception `fc::exception` if a wallet not found or locked.
   */
  std::vector<std::string> import_keys( const std::string& token, const std::string& wallet_name, const std::vector<std::string>& wif_keys, const std::string& prefix );

  /**
   * 
   * Removes a private key from specified wallet.
   * Wallet must be opened and unlocked.
   * @param token       Session's identifier.
   * @param wallet_name A name of a wallet to remove a private key from.
   * @param public_key  The public key to find corresponding a private key to remove, e.g. STM51AaVqQTG1rUUWvgWxGDZrRGPHTW85grXhUqWCyuAYEh8fyfjm.
   * @throws            An exception `fc::exception` if a wallet not found or locked or a private key is not removed.
   */
  void remove_key( const std::string& token, const std::string& wallet_name, const public_key_type& public_key );

  /**
   * Gets current time and timeout time
   * 
   * @param token Session's identifier.
   * @return      Current time and timeout time.
   */
  info get_info( const std::string& token );

  /**
   * Gets current beekeeper version
   * 
   * @return      Current version based on git hash
   */
  version get_version();

  /** Create a session by a token's generating. That token is used in every endpoint that requires an unlocking wallet.
   *
   * @param salt                    Random data that is used as an additional input so as to create a token. Optional.
   * @param notifications_endpoint  A server attached to given session. It's used to receive notifications. Optional.
   * @returns                       A token that is attached to newly created session.
   */
  std::string create_session( const std::optional<std::string>& salt, const std::optional<std::string>& notifications_endpoint );

  /** Close a session according to given token.
   * After closing the session expires.
   * 
   * @param token  Session's identifier.
   * @param allow_close_all_sessions_action If `true`, after closing a session earlier defined action is executed.
   */
  void close_session( const std::string& token, bool allow_close_all_sessions_action = true );

  /**
   * 
   * Tests if a private key corresponding to a public key exists in a wallet
   * The wallet must be opened and unlocked.
   * @param token       Session's identifier.
   * @param wallet_name A name of a wallet.
   * @param public_key  A public key corresponding to a private key that is stored in a wallet.
   * @returns           A `true` value if a private key exists otherwise `false`.
   */
  bool has_matching_private_key( const std::string& token, const std::string& wallet_name, const public_key_type& public_key );

  /** Encrypts given content.
   *
   * Using public key of creator and public key of receiver, content is encrypted.
   * @param token             Session's identifier.
   * @param from_public_key   A public key of creator.
   * @param to_public_key     A public key of receiver.
   * @param wallet_name       A name of a wallet to find a key corresponding to `from_public_key` and `to_public_key`.
   * @param content           A string to encrypt.
   * @param nonce             Optional nonce to be used for encryption (otherwise current time is used).
   * @param prefix            A prefix of a public key
   * @returns                 An encrypted string.
   */
  std::string encrypt_data( const std::string& token, const public_key_type& from_public_key, const public_key_type& to_public_key, const std::string& wallet_name, const std::string& content, const std::optional<unsigned int>& nonce, const std::string& prefix );

  /** Decrypts given content.
   *
   * When private keys are accessible (exist in unlocked wallets) content is decrypted.
   * @param token             Session's identifier.
   * @param from_public_key   A public key of creator.
   * @param to_public_key     A public key of receiver.
   * @param wallet_name       A name of the wallet to find a key corresponding to `from_public_key` or `to_public_key`. Only one private key is necessary.
   * @param encrypted_content A string to decrypt.
   * @returns                 A decrypted string.
   */
  std::string decrypt_data( const std::string& token, const public_key_type& from_public_key, const public_key_type& to_public_key, const std::string& wallet_name, const std::string& encrypted_content );

  /**
   * 
   * Tests if a wallet exists
   * @param token       Session's identifier.
   * @param wallet_name A name of a wallet.
   * @returns           A `true` value if a wallet exists otherwise `false`.
   */
  bool has_wallet( const std::string& token, const std::string& wallet_name );

private:

  seconds_type unlock_timeout = 900;

  uint32_t  session_cnt   = 0;
  uint32_t  session_limit = 0;

  const boost::filesystem::path wallet_directory;

  close_all_sessions_action_method  close_all_sessions_action;

  std::shared_ptr<session_manager_base> sessions;
  std::shared_ptr<beekeeper_instance_base> instance;
};

} //beekeeper
