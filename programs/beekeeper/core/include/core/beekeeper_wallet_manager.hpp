#pragma once

#include <core/beekeeper_wallet_base.hpp>
#include <core/beekeeper_instance_base.hpp>
#include <core/session_manager_base.hpp>

#include<csignal>

namespace beekeeper {

/// Provides associate of wallet name to wallet and manages the interaction with each wallet.
///
/// The name of the wallet is also used as part of the file name by soft_wallet. See beekeeper_wallet_manager::create.
/// No const methods because timeout may cause lock_all() to be called.
class beekeeper_wallet_manager
{
private:

  using close_all_sessions_action_method = std::function<void()>;

public:

  /// Set the path for location of wallet files.
  /// @param command_line_wallet_dir path to override default ./ location of wallet files.
  /// @param command_line_unlock_timeout timeout for unlocked wallet [s]
  /// @param method an action that will be executed when all sessions are closed
  beekeeper_wallet_manager( std::shared_ptr<session_manager_base> sessions, std::shared_ptr<beekeeper_instance_base> instance, const boost::filesystem::path& cmd_wallet_dir, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit,
                            close_all_sessions_action_method&& method = [](){ std::raise(SIGINT); } );

  beekeeper_wallet_manager(const beekeeper_wallet_manager&) = delete;
  beekeeper_wallet_manager(beekeeper_wallet_manager&&) = delete;
  beekeeper_wallet_manager& operator=(const beekeeper_wallet_manager&) = delete;
  beekeeper_wallet_manager& operator=(beekeeper_wallet_manager&&) = delete;

  ~beekeeper_wallet_manager(){}

  bool start();

  /// Set the timeout for locking all wallets.
  /// If set then after t seconds of inactivity then lock_all().
  /// Activity is defined as any beekeeper_wallet_manager method call below.
  /// @param secs The timeout in seconds.
  void set_timeout( const std::string& token, seconds_type seconds );

  /// Sign digest with the private keys specified via their public keys.
  /// @param digest the digest to sign.
  /// @param key the public key of the corresponding private key to sign the digest with
  /// @return signature over the digest
  /// @throws fc::exception if corresponding private keys not found in unlocked wallets
  signature_type sign_digest( const std::string& token, const digest_type& sig_digest, const std::string& public_key );

  /// Sign a binary transaction with the private keys specified via their public keys.
  /// @param token represents a session
  /// @param transaction the transaction in binary form.
  /// @param chain_id the chain_id to sign transaction with.
  /// @param public_key the public key of the corresponding private key to sign the digest with
  /// @return signature over the transaction
  /// @throws fc::exception if corresponding private keys not found in unlocked wallets
  signature_type sign_binary_transaction( const std::string& token, const string& transaction, const chain_id_type& chain_id, const std::string& public_key );

  /// Sign a JSON transaction with the private keys specified via their public keys.
  /// @param token represents a session
  /// @param transaction the transaction in binary JSON.
  /// @param chain_id the chain_id to sign transaction with.
  /// @param public_key the public key of the corresponding private key to sign the digest with
  /// @return signature over the transaction
  /// @throws fc::exception if corresponding private keys not found in unlocked wallets
  signature_type sign_transaction( const std::string& token, const string& transaction, const chain_id_type& chain_id, const std::string& public_key );

  /// Create a new wallet.
  /// A new wallet is created in file dir/{name}.wallet see set_dir.
  /// The new wallet is unlocked after creation.
  /// @param name of the wallet and name of the file without ext .wallet.
  /// @param password to set for wallet, if not given will be automatically generated
  /// @return Plaintext password that is needed to unlock wallet. Caller is responsible for saving password otherwise
  ///      they will not be able to unlock their wallet. Note user supplied passwords are not supported.
  /// @throws fc::exception if wallet with name already exists (or filename already exists)
  std::string create( const std::string& token, const std::string& name, fc::optional<std::string> password = fc::optional<std::string>{} );

  /// Open an existing wallet file dir/{name}.wallet.
  /// Note this does not unlock the wallet, see beekeeper_wallet_manager::unlock.
  /// @param name of the wallet file (minus ext .wallet) to open.
  /// @throws fc::exception if unable to find/open the wallet file.
  void open( const std::string& token, const std::string& name );

  /// Close an existing wallet file dir/{name}.wallet.
  /// Note this locks the wallet, see beekeeper_wallet_manager::lock.
  /// @param name of the wallet file (minus ext .wallet) to close.
  /// @throws fc::exception if unable to find/close the wallet file.
  void close( const std::string& token, const std::string& name );

  /// @return A list of wallet names with " *" appended if the wallet is unlocked.
  std::vector<wallet_details> list_wallets( const std::string& token );

  /// @return A list of private keys from a wallet provided password is correct to said wallet
  map<public_key_type, private_key_type> list_keys( const std::string& token, const string& name, const string& pw );

  /// @return A set of public keys from all unlocked wallets, use with chain_controller::get_required_keys.
  flat_set<public_key_type> get_public_keys( const std::string& token );

  /// Locks all the unlocked wallets.
  void lock_all( const std::string& token );

  /// Lock the specified wallet.
  /// No-op if wallet already locked.
  /// @param name the name of the wallet to lock.
  /// @throws fc::exception if wallet with name not found.
  void lock( const std::string& token, const std::string& name );

  /// Unlock the specified wallet.
  /// The wallet remains unlocked until ::lock is called or program exit.
  /// @param name the name of the wallet to lock.
  /// @param password the plaintext password returned from ::create.
  /// @throws fc::exception if wallet not found or invalid password or already unlocked.
  void unlock( const std::string& token, const std::string& name, const std::string& password );

  /// Import private key into specified wallet.
  /// Imports a WIF Private Key into specified wallet.
  /// Wallet must be opened and unlocked.
  /// @param name the name of the wallet to import into.
  /// @param wif_key the WIF Private Key to import, e.g. 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n
  /// @throws fc::exception if wallet not found or locked.
  string import_key( const std::string& token, const std::string& name, const std::string& wif_key );

  /// Removes a key from the specified wallet.
  /// Wallet must be opened and unlocked.
  /// @param name the name of the wallet to remove the key from.
  /// @param password the plaintext password returned from ::create.
  /// @param public_key the Public Key to remove, e.g. STM51AaVqQTG1rUUWvgWxGDZrRGPHTW85grXhUqWCyuAYEh8fyfjm
  /// @throws fc::exception if wallet not found or locked or key is not removed.
  void remove_key( const std::string& token, const std::string& name, const std::string& password, const std::string& public_key );

  /// @return Current time and timeout time
  info get_info( const std::string& token );

  /** Create a session by token generating. That token is used in every endpoint that requires unlocking wallet.
   *
   * @param salt random data that is used as an additional input so as to create token. Can be empty.
   * @param notifications_endpoint a server attached to given session. It's used to receive notifications. Can be empty.
   * @returns a token that is attached to newly created session
   */
  string create_session( const string& salt, const string& notifications_endpoint );

  /** Close a session according to given token.
   *
   * @param token represents a session. After closing the session expires.
   */
  void close_session( const string& token );

private:

  seconds_type unlock_timeout = 900;

  uint32_t  session_cnt   = 0;
  uint32_t  session_limit = 0;

  uint32_t public_key_size = 0;

  close_all_sessions_action_method  close_all_sessions_action;

  std::shared_ptr<session_manager_base> sessions;
  std::shared_ptr<beekeeper_instance_base> instance;

  public_key_type create_public_key( const std::string& public_key );
};

} //beekeeper
