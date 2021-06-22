#pragma once

#include <hive/plugins/wallet_bridge_api/wallet_bridge_api.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <fc/macros.hpp>
#include <fc/real128.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/api.hpp>

namespace hive { namespace wallet {

template<typename T>
struct serializer_wrapper
{
  T value;
};

using namespace std;

using namespace hive::utilities;
using namespace hive::protocol;
using namespace hive::plugins;

typedef uint16_t transaction_handle_type;

struct memo_data {

  static optional<memo_data> from_string( string str ) {
    try {
      if( str.size() > sizeof(memo_data) && str[0] == '#') {
        auto data = fc::from_base58( str.substr(1) );
        auto m  = fc::raw::unpack_from_vector<memo_data>( data );
        FC_ASSERT( string(m) == str );
        return m;
      }
    } catch ( ... ) {}
    return optional<memo_data>();
  }

  public_key_type from;
  public_key_type to;
  uint64_t        nonce = 0;
  uint32_t        check = 0;
  vector<char>    encrypted;

  operator string()const {
    auto data = fc::raw::pack_to_vector(*this);
    auto base58 = fc::to_base58( data );
    return '#'+base58;
  }
};



struct brain_key_info
{
  string               brain_priv_key;
  public_key_type      pub_key;
  string               wif_priv_key;
};

struct wallet_data
{
  vector<char>              cipher_keys; /** encrypted keys */

  string                    ws_server = "ws://localhost:8090";
  string                    ws_user;
  string                    ws_password;
};

enum authority_type
{
  owner,
  active,
  posting
};

namespace detail {
class wallet_api_impl;
}

/**
  * This wallet assumes it is connected to the database server with a high-bandwidth, low-latency connection and
  * performs minimal caching. This API could be provided locally to be used by a web interface.
  */
class wallet_api
{
  public:
    wallet_api( const wallet_data& initial_data, const chain_id_type& _hive_chain_id, const fc::api< hive::plugins::wallet_bridge_api::wallet_bridge_api >& remote_api);
    virtual ~wallet_api();

    bool copy_wallet_file( const string& destination_filename );


    /** Returns a list of all commands supported by the wallet API.
      *
      * This lists each command, along with its arguments and return types.
      * For more detailed help on a single command, use \c get_help()
      *
      * @returns a multi-line string suitable for displaying on a terminal
      */
    string                              help()const;

    /**
      * Returns info about the current state of the blockchain
      */
    variant                             info();

    /** Returns info such as client version, git version of graphene/fc, version of boost, openssl.
      * @returns compile time info and client and dependencies versions
      */
    variant_object                      about() const;

    /** Returns the information about a block
      *
      * @param num Block num
      *
      * @returns Public block data on the blockchain
      */
    optional<serializer_wrapper<block_api::api_signed_block_object>> get_block( uint32_t num );

    /** Returns sequence of operations included/generated in a specified block
      *
      * @param block_num Block height of specified block
      * @param only_virtual Whether to only return virtual operations
      */
    serializer_wrapper<vector< account_history::api_operation_object >> get_ops_in_block( uint32_t block_num, bool only_virtual = true );

    /** Return the current price feed history
      *
      * @returns Price feed history data on the blockchain
      */
    serializer_wrapper<database_api::api_feed_history_object> get_feed_history()const;

    /**
      * Returns the list of witnesses producing blocks in the current round (21 Blocks)
      */
    vector< account_name_type > get_active_witnesses()const;

    /**
      * Returns vesting withdraw routes for an account.
      *
      * @param account Account to query routes
      * @param type Withdraw type type [incoming, outgoing, all]
      */
    vector< database_api::api_withdraw_vesting_route_object > get_withdraw_routes( const string& account, database_api::sort_order_type sort_type = database_api::by_withdraw_route )const;

    /**
      *  Gets the account information for all accounts for which this wallet has a private key
      */
    serializer_wrapper<vector< database_api::api_account_object >> list_my_accounts();

    /** Lists all accounts registered in the blockchain.
      * This returns a list of all account names and their account ids, sorted by account name.
      *
      * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all accounts,
      * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
      * the last account name returned as the \c lowerbound for the next \c list_accounts() call.
      *
      * @param lowerbound the name of the first account to return.  If the named account does not exist,
      *                   the list will start at the account that comes after \c lowerbound
      * @param limit the maximum number of accounts to return (max: 1000)
      * @returns a list of accounts.
      */
    vector< account_name_type > list_accounts(const string& lowerbound, uint32_t limit);

    /** Returns the block chain's rapidly-changing properties.
      * The returned object contains information that changes every block interval
      * such as the head block number, the next witness, etc.
      * @see \c get_global_properties() for less-frequently changing properties
      * @returns the dynamic global properties
      */
    database_api::api_dynamic_global_property_object get_dynamic_global_properties() const;

    /** Returns information about the given account.
      *
      * @param account_name the name of the account to provide information about
      * @returns the public account data stored in the blockchain
      */
    serializer_wrapper<database_api::api_account_object> get_account( const string& account_name ) const;

    /** Returns information about the given accounts.
      *
      * @param account_name the names of the accounts to provide information about
      * @returns the public account data stored in the blockchain
      */
    serializer_wrapper<vector<database_api::api_account_object>> get_accounts( const vector<string>& account_names ) const;

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
    string                            get_private_key( public_key_type pubkey )const;

    /**
      *  @param account  - the name of the account to retrieve key for
      *  @param role     - active | owner | posting | memo
      *  @param password - the password to be used at key generation
      *  @return public key corresponding to generated private key, and private key in WIF format.
      */
    pair<public_key_type,string>  get_private_key_from_password( const string& account, const string& role, const string& password )const;


    /**
      * Returns transaction by ID.
      */
    serializer_wrapper<annotated_signed_transaction> get_transaction( transaction_id_type trx_id )const;

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
    void    unlock(const string& password);

    /** Sets a new password on the wallet.
      *
      * The wallet must be either 'new' or 'unlocked' to
      * execute this command.
      * @ingroup Wallet Management
      */
    void    set_password(const string& password);

    /** Dumps all private keys owned by the wallet.
      *
      * The keys are printed in WIF format.  You can import these keys into another wallet
      * using \c import_key()
      * @returns a map containing the private keys, indexed by their public key
      */
    map<public_key_type, string> list_keys();

    /** Returns detailed help on a single API command.
      * @param method the name of the API command you want help with
      * @returns a multi-line string suitable for displaying on a terminal
      */
    string  gethelp(const string& method)const;

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

    /** Suggests a safe brain key to use for creating your account.
      * \c create_account_with_brain_key() requires you to specify a 'brain key',
      * a long passphrase that provides enough entropy to generate cyrptographic
      * keys.  This function will suggest a suitably random string that should
      * be easy to write down (and, with effort, memorize).
      * @returns a suggested brain_key
      */
    brain_key_info suggest_brain_key()const;

    /** Converts a signed_transaction in JSON form to its binary representation.
      *
      * TODO: I don't see a broadcast_transaction() function, do we need one?
      *
      * @param tx the transaction to serialize
      * @returns the binary form of the transaction.  It will not be hex encoded,
      *          this returns a raw string that may have null characters embedded
      *          in it
      */
    string serialize_transaction( const serializer_wrapper<signed_transaction>& tx) const;

    /** Imports a WIF Private Key into the wallet to be used to sign transactions by an account.
      *
      * example: import_key 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3
      *
      * @param wif_key the WIF Private Key to import
      */
    bool import_key( const string& wif_key );

    /** Transforms a brain key to reduce the chance of errors when re-entering the key from memory.
      *
      * This takes a user-supplied brain key and normalizes it into the form used
      * for generating private keys.  In particular, this upper-cases all ASCII characters
      * and collapses multiple spaces into one.
      * @param s the brain key as supplied by the user
      * @returns the brain key in its normalized form
      */
    string normalize_brain_key(string s) const;

    /**
     *  This method will claim a subsidized account creation.
     *
     *  @param creator The account to receive the account creation credit
     *  @param fee The fee to pay for claiming the account (either 0 steem for a discounted account, or the full account fee)
     *  @param broadcast true if you wish to broadcast the transaction
     */
    serializer_wrapper<annotated_signed_transaction> claim_account_creation( const string& creator,
                                                                     const serializer_wrapper<hive::protocol::asset>& fee,
                                                                     bool broadcast )const; 
    /**
     *  This method will claim a subsidized account creation without waiting for the transaction to confirm.
     *
     *  @param creator The account to receive the account creation credit
     *  @param fee The fee to pay for claiming the account (either 0 steem for a discounted account, or the full account fee)
     *  @param broadcast true if you wish to broadcast the transaction
     */
    serializer_wrapper<annotated_signed_transaction> claim_account_creation_nonblocking( const string& creator,
                                                                                 const serializer_wrapper<hive::protocol::asset>& fee,
                                                                                 bool broadcast )const;
       

    /**
      *  This method will genrate new owner, active, and memo keys for the new account which
      *  will be controlable by this wallet. There is a fee associated with account creation
      *  that is paid by the creator. The current account creation fee can be found with the
      *  'info' wallet command.
      *
      *  @param creator The account creating the new account
      *  @param new_account_name The name of the new account
      *  @param json_meta JSON Metadata associated with the new account
      *  @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> create_account( const string& creator, const string& new_account_name, const string& json_meta, bool broadcast );

    /**
      * This method is used by faucets to create new accounts for other users which must
      * provide their desired keys. The resulting account may not be controllable by this
      * wallet. There is a fee associated with account creation that is paid by the creator.
      * The current account creation fee can be found with the 'info' wallet command.
      *
      * @param creator The account creating the new account
      * @param newname The name of the new account
      * @param json_meta JSON Metadata associated with the new account
      * @param owner public owner key of the new account
      * @param active public active key of the new account
      * @param posting public posting key of the new account
      * @param memo public memo key of the new account
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> create_account_with_keys(
      const string& creator,
      const string& newname,
      const string& json_meta,
      public_key_type owner,
      public_key_type active,
      public_key_type posting,
      public_key_type memo,
      bool broadcast )const;

    
    /**
     * This method is used by faucets to create new accounts for other users which must
     * provide their desired keys. The resulting account may not be controllable by this
     * wallet. There is a fee associated with account creation that is paid by the creator.
     * The current account creation fee can be found with the 'info' wallet command.
     * This method is identical to create_account_with_keys() except that it also 
     * adds a 'transfer' operation to the transaction after the create.
     *
     * @param creator The account creating the new account
     * @param newname The name of the new account
     * @param initial_amount The amount transferred to the account
     * @param memo A memo to send with the transfer
     * @param json_meta JSON Metadata associated with the new account
     * @param owner public owner key of the new account
     * @param active public active key of the new account
     * @param posting public posting key of the new account
     * @param memo public memo key of the new account
     * @param broadcast true if you wish to broadcast the transaction
     */
    serializer_wrapper<annotated_signed_transaction> create_funded_account_with_keys( const string& creator,
                                                                              const string& new_account_name,
                                                                              const serializer_wrapper<hive::protocol::asset>& initial_amount,
                                                                              const string& memo,
                                                                              const string& json_meta,
                                                                              public_key_type owner_key,
                                                                              public_key_type active_key,
                                                                              public_key_type posting_key,
                                                                              public_key_type memo_key,
                                                                              bool broadcast )const;
    
    /**
      *  This method will genrate new owner, active, and memo keys for the new account which
      *  will be controlable by this wallet. There is a fee associated with account creation
      *  that is paid by the creator. The current account creation fee can be found with the
      *  'info' wallet command.
      *
      *  These accounts are created with combination of HIVE and delegated SP
      *
      *  @param creator The account creating the new account
      *  @param hive_fee The amount of the fee to be paid with HIVE
      *  @param delegated_vests The amount of the fee to be paid with delegation
      *  @param new_account_name The name of the new account
      *  @param json_meta JSON Metadata associated with the new account
      *  @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> create_account_delegated(
      const string& creator,
      const serializer_wrapper<hive::protocol::asset>& hive_fee,
      const serializer_wrapper<hive::protocol::asset>& delegated_vests,
      const string& new_account_name,
      const string& json_meta,
      bool broadcast );

    /**
      * This method is used by faucets to create new accounts for other users which must
      * provide their desired keys. The resulting account may not be controllable by this
      * wallet. There is a fee associated with account creation that is paid by the creator.
      * The current account creation fee can be found with the 'info' wallet command.
      *
      * These accounts are created with combination of HIVE and delegated SP
      *
      * @param creator The account creating the new account
      * @param hive_fee The amount of the fee to be paid with HIVE
      * @param delegated_vests The amount of the fee to be paid with delegation
      * @param newname The name of the new account
      * @param json_meta JSON Metadata associated with the new account
      * @param owner public owner key of the new account
      * @param active public active key of the new account
      * @param posting public posting key of the new account
      * @param memo public memo key of the new account
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> create_account_with_keys_delegated(
      const string& creator,
      const serializer_wrapper<hive::protocol::asset>& hive_fee,
      const serializer_wrapper<hive::protocol::asset>& delegated_vests,
      const string& newname,
      const string& json_meta,
      public_key_type owner,
      public_key_type active,
      public_key_type posting,
      public_key_type memo,
      bool broadcast )const;

    /**
      * This method updates the keys of an existing account.
      *
      * @param accountname The name of the account
      * @param json_meta New JSON Metadata to be associated with the account
      * @param owner New public owner key for the account
      * @param active New public active key for the account
      * @param posting New public posting key for the account
      * @param memo New public memo key for the account
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> update_account(
      const string& accountname,
      const string& json_meta,
      public_key_type owner,
      public_key_type active,
      public_key_type posting,
      public_key_type memo,
      bool broadcast )const;

    /**
      * This method updates the key of an authority for an exisiting account.
      * Warning: You can create impossible authorities using this method. The method
      * will fail if you create an impossible owner authority, but will allow impossible
      * active and posting authorities.
      *
      * @param account_name The name of the account whose authority you wish to update
      * @param type The authority type. e.g. owner, active, or posting
      * @param key The public key to add to the authority
      * @param weight The weight the key should have in the authority. A weight of 0 indicates the removal of the key.
      * @param broadcast true if you wish to broadcast the transaction.
      */
    serializer_wrapper<annotated_signed_transaction> update_account_auth_key(
      const string& account_name,
      authority_type type,
      public_key_type key,
      weight_type weight,
      bool broadcast );

    /**
      * This method updates the account of an authority for an exisiting account.
      * Warning: You can create impossible authorities using this method. The method
      * will fail if you create an impossible owner authority, but will allow impossible
      * active and posting authorities.
      *
      * @param account_name The name of the account whose authority you wish to update
      * @param type The authority type. e.g. owner, active, or posting
      * @param auth_account The account to add the the authority
      * @param weight The weight the account should have in the authority. A weight of 0 indicates the removal of the account.
      * @param broadcast true if you wish to broadcast the transaction.
      */
    serializer_wrapper<annotated_signed_transaction> update_account_auth_account(
      const string& account_name,
      authority_type type,
      const string& auth_account,
      weight_type weight,
      bool broadcast );

    /**
      * This method updates the weight threshold of an authority for an account.
      * Warning: You can create impossible authorities using this method as well
      * as implicitly met authorities. The method will fail if you create an implicitly
      * true authority and if you create an impossible owner authoroty, but will allow
      * impossible active and posting authorities.
      *
      * @param account_name The name of the account whose authority you wish to update
      * @param type The authority type. e.g. owner, active, or posting
      * @param threshold The weight threshold required for the authority to be met
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> update_account_auth_threshold(
      const string& account_name,
      authority_type type,
      uint32_t threshold,
      bool broadcast );

    /**
      * This method updates the account JSON metadata
      *
      * @param account_name The name of the account you wish to update
      * @param json_meta The new JSON metadata for the account. This overrides existing metadata
      * @param broadcast ture if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> update_account_meta(
      const string& account_name,
      const string& json_meta,
      bool broadcast );

    /**
      * This method updates the memo key of an account
      *
      * @param account_name The name of the account you wish to update
      * @param key The new memo public key
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> update_account_memo_key(
      const string& account_name,
      public_key_type key,
      bool broadcast );


    /**
      * This method delegates VESTS from one account to another.
      *
      * @param delegator The name of the account delegating VESTS
      * @param delegatee The name of the account receiving VESTS
      * @param vesting_shares The amount of VESTS to delegate
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> delegate_vesting_shares(
      const string& delegator,
      const string& delegatee,
      const serializer_wrapper<hive::protocol::asset>& vesting_shares,
      bool broadcast );


    serializer_wrapper<annotated_signed_transaction> delegate_vesting_shares_nonblocking(
      const string& delegator,
      const string& delegatee,
      const serializer_wrapper<hive::protocol::asset>& vesting_shares,
      bool broadcast );

    // these versions also send a regular transfer in the same transaction, intended for sending a .001 STEEM memo
    serializer_wrapper<annotated_signed_transaction> delegate_vesting_shares_and_transfer(
      const string& delegator,
      const string& delegatee,
      const serializer_wrapper<hive::protocol::asset>& vesting_shares,
      const serializer_wrapper<hive::protocol::asset>& transfer_amount,
      optional<string> transfer_memo,
      bool broadcast );

    serializer_wrapper<annotated_signed_transaction> delegate_vesting_shares_and_transfer_nonblocking(
      const string& delegator,
      const string& delegatee,
      const serializer_wrapper<hive::protocol::asset>& vesting_shares,
      const serializer_wrapper<hive::protocol::asset>& transfer_amount,
      optional<string> transfer_memo,
      bool broadcast );

    // helper function
    serializer_wrapper<annotated_signed_transaction> delegate_vesting_shares_and_transfer_and_broadcast(
      const string& delegator, const string& delegatee, const hive::protocol::asset& vesting_shares, 
      optional<hive::protocol::asset> transfer_amount, optional<string> transfer_memo,
      bool broadcast, bool blocking );

    /**
      *  This method is used to convert a JSON transaction to its transaction ID.
      */
    transaction_id_type get_transaction_id( const signed_transaction& trx )const { return trx.id(); }

    /** Lists all witnesses registered in the blockchain.
      * This returns a list of all account names that own witnesses, and the associated witness id,
      * sorted by name.  This lists witnesses whether they are currently voted in or not.
      *
      * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all witnesss,
      * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
      * the last witness name returned as the \c lowerbound for the next \c list_witnesss() call.
      *
      * @param lowerbound the name of the first witness to return.  If the named witness does not exist,
      *                   the list will start at the witness that comes after \c lowerbound
      * @param limit the maximum number of witnesss to return (max: 1000)
      * @returns a list of witnesss mapping witness names to witness ids
      */
    vector< account_name_type > list_witnesses(const string& lowerbound, uint32_t limit);

    /** Returns information about the given witness.
      * @param owner_account the name or id of the witness account owner, or the id of the witness
      * @returns the information about the witness stored in the block chain
      */
    optional<serializer_wrapper<database_api::api_witness_object>> get_witness(const string& owner_account);

    /** Returns conversion requests by an account
      *
      * @param owner Account name of the account owning the requests
      *
      * @returns All pending conversion requests by account
      */
    serializer_wrapper<vector< database_api::api_convert_request_object >> get_conversion_requests( const string& owner );

    /** Returns collateralized conversion requests by an account
      *
      * @param owner Account name of the account owning the requests
      *
      * @returns All pending collateralized conversion requests by account
      */
    serializer_wrapper<vector< database_api::api_collateralized_convert_request_object >> get_collateralized_conversion_requests( const string& owner );

    /**
      * Update a witness object owned by the given account.
      *
      * @param witness_name The name of the witness account.
      * @param url A URL containing some information about the witness.  The empty string makes it remain the same.
      * @param block_signing_key The new block signing public key.  The empty string disables block production.
      * @param props The chain properties the witness is voting on.
      * @param broadcast true if you wish to broadcast the transaction.
      */
    serializer_wrapper<annotated_signed_transaction> update_witness(
      const string& witness_name,
      const string& url,
      public_key_type block_signing_key,
      const serializer_wrapper<legacy_chain_properties>& props,
      bool broadcast = false);

    /** Set the voting proxy for an account.
      *
      * If a user does not wish to take an active part in voting, they can choose
      * to allow another account to vote their stake.
      *
      * Setting a vote proxy does not remove your previous votes from the blockchain,
      * they remain there but are ignored.  If you later null out your vote proxy,
      * your previous votes will take effect again (ABW: definitely not true with regard to witness votes).
      *
      * This setting can be changed at any time.
      *
      * @param account_to_modify the name or id of the account to update
      * @param proxy the name of account that should proxy to, or empty string to have no proxy
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> set_voting_proxy(
      const string& account_to_modify,
      const string& proxy,
      bool broadcast = false);

    /**
      * Vote for a witness to become a block producer. By default an account has not voted
      * positively or negatively for a witness. The account can either vote for with positively
      * votes or against with negative votes. The vote will remain until updated with another
      * vote. Vote strength is determined by the accounts vesting shares.
      * ABW: not true; there are no negative witness votes, you can only remove previously cast vote
      *
      * @param account_to_vote_with The account voting for a witness
      * @param witness_to_vote_for The witness that is being voted for
      * @param approve true if the account is voting for the account to be able to be a block produce
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> vote_for_witness(
      const string& account_to_vote_with,
      const string& witness_to_vote_for,
      bool approve = true,
      bool broadcast = false);

    /**
      * Transfer funds from one account to another. HIVE and HBD can be transferred.
      *
      * @param from The account the funds are coming from
      * @param to The account the funds are going to
      * @param amount The funds being transferred. i.e. "100.000 HIVE"
      * @param memo A memo for the transaction, encrypted with the to account's public memo key
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> transfer(
      const string& from,
      const string& to,
      const serializer_wrapper<hive::protocol::asset>& amount,
      const string& memo,
      bool broadcast = false);

    /**
      * Transfer funds from one account to another using escrow. HIVE and HBD can be transferred.
      *
      * @param from The account the funds are coming from
      * @param to The account the funds are going to
      * @param agent The account acting as the agent in case of dispute
      * @param escrow_id A unique id for the escrow transfer. (from, escrow_id) must be a unique pair
      * @param hbd_amount The amount of HBD to transfer
      * @param hive_amount The amount of HIVE to transfer
      * @param fee The fee paid to the agent
      * @param ratification_deadline The deadline for 'to' and 'agent' to approve the escrow transfer
      * @param escrow_expiration The expiration of the escrow transfer, after which either party can claim the funds
      * @param json_meta JSON encoded meta data
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> escrow_transfer(
      const string& from,
      const string& to,
      const string& agent,
      uint32_t escrow_id,
      const serializer_wrapper<hive::protocol::asset>& hbd_amount,
      const serializer_wrapper<hive::protocol::asset>& hive_amount,
      const serializer_wrapper<hive::protocol::asset>& fee,
      const time_point_sec& ratification_deadline,
      const time_point_sec& escrow_expiration,
      const string& json_meta,
      bool broadcast = false
    );

    /**
      * Approve a proposed escrow transfer. Funds cannot be released until after approval. This is in lieu of requiring
      * multi-sig on escrow_transfer
      *
      * @param from The account that funded the escrow
      * @param to The destination of the escrow
      * @param agent The account acting as the agent in case of dispute
      * @param who The account approving the escrow transfer (either 'to' or 'agent)
      * @param escrow_id A unique id for the escrow transfer
      * @param approve true to approve the escrow transfer, otherwise cancels it and refunds 'from'
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> escrow_approve(
      const string& from,
      const string& to,
      const string& agent,
      const string& who,
      uint32_t escrow_id,
      bool approve,
      bool broadcast = false
    );

    /**
      * Raise a dispute on the escrow transfer before it expires
      *
      * @param from The account that funded the escrow
      * @param to The destination of the escrow
      * @param agent The account acting as the agent in case of dispute
      * @param who The account raising the dispute (either 'from' or 'to')
      * @param escrow_id A unique id for the escrow transfer
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> escrow_dispute(
      const string& from,
      const string& to,
      const string& agent,
      const string& who,
      uint32_t escrow_id,
      bool broadcast = false
    );

    /**
      * Release funds help in escrow
      *
      * @param from The account that funded the escrow
      * @param to The account the funds are originally going to
      * @param agent The account acting as the agent in case of dispute
      * @param who The account authorizing the release
      * @param receiver The account that will receive funds being released
      * @param escrow_id A unique id for the escrow transfer
      * @param hbd_amount The amount of HBD that will be released
      * @param hive_amount The amount of HIVE that will be released
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> escrow_release(
      const string& from,
      const string& to,
      const string& agent,
      const string& who,
      const string& receiver,
      uint32_t escrow_id,
      const serializer_wrapper<hive::protocol::asset>& hbd_amount,
      const serializer_wrapper<hive::protocol::asset>& hive_amount,
      bool broadcast = false
    );

    /**
      * Transfer HIVE into a vesting fund represented by vesting shares (VESTS). VESTS are required to vesting
      * for a minimum of one coin year and can be withdrawn once a week over a two year withdraw period.
      * VESTS are protected against dilution up until 90% of HIVE is vesting.
      *
      * @param from The account the HIVE is coming from
      * @param to The account getting the VESTS
      * @param amount The amount of HIVE to vest i.e. "100.00 HIVE"
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> transfer_to_vesting(
      const string& from,
      const string& to,
      const serializer_wrapper<hive::protocol::asset>& amount,
      bool broadcast = false);


    /**
     * Transfer funds from one account to another, without waiting for a confirmation. STEEM and SBD can be transferred.
     *
     * @param from The account the funds are coming from
     * @param to The account the funds are going to
     * @param amount The funds being transferred. i.e. "100.000 STEEM"
     * @param memo A memo for the transactionm, encrypted with the to account's public memo key
     * @param broadcast true if you wish to broadcast the transaction
     */
    serializer_wrapper<annotated_signed_transaction> transfer_nonblocking(const string& from, const string& to,
      const serializer_wrapper<hive::protocol::asset>& amount, const string& memo, bool broadcast = false);

    // helper function
    serializer_wrapper<annotated_signed_transaction> transfer_and_broadcast(const string& from, const string& to,
      const hive::protocol::asset& amount, const string& memo, bool broadcast, bool blocking );
    /*
     * Transfer STEEM into a vesting fund represented by vesting shares (VESTS) without waiting for a confirmation.
     * VESTS are required to vesting
     * for a minimum of one coin year and can be withdrawn once a week over a two year withdraw period.
     * VESTS are protected against dilution up until 90% of STEEM is vesting.
     *
     * @param from The account the STEEM is coming from
     * @param to The account getting the VESTS
     * @param amount The amount of STEEM to vest i.e. "100.00 STEEM"
     * @param broadcast true if you wish to broadcast the transaction
     */
    serializer_wrapper<annotated_signed_transaction> transfer_to_vesting_nonblocking(const string& from, const string& to,
      const serializer_wrapper<hive::protocol::asset>& amount, bool broadcast = false);

    // helper function
    serializer_wrapper<annotated_signed_transaction> transfer_to_vesting_and_broadcast(const string& from, const string& to,
      const hive::protocol::asset& amount, bool broadcast, bool blocking );


    /**
      *  Transfers into savings happen immediately, transfers from savings take 72 hours
      */
    serializer_wrapper<annotated_signed_transaction> transfer_to_savings(
      const string& from,
      const string& to,
      const serializer_wrapper<hive::protocol::asset>& amount,
      const string& memo,
      bool broadcast = false );

    /**
      *  @param from       - the account that initiated the transfer
      *  @param request_id - an unique ID assigned by from account, the id is used to cancel the operation and can be reused after the transfer completes
      *  @param to         - the account getting the transfer
      *  @param amount     - the amount of assets to be transfered
      *  @param memo A memo for the transaction, encrypted with the to account's public memo key
      *  @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> transfer_from_savings(
      const string& from,
      uint32_t request_id,
      const string& to,
      const serializer_wrapper<hive::protocol::asset>& amount,
      const string& memo,
      bool broadcast = false );

    /**
      *  @param from the account that initiated the transfer
      *  @param request_id the id used in transfer_from_savings
      *  @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> cancel_transfer_from_savings(
      const string& from,
      uint32_t request_id,
      bool broadcast = false );

    /**
      * Set up a vesting withdraw request. The request is fulfilled once a week over the next 13 weeks.
      *
      * @param from The account the VESTS are withdrawn from
      * @param vesting_shares The amount of VESTS to withdraw over the next 13 weeks. Each week (amount/13) shares are
      *    withdrawn and deposited back as HIVE. i.e. "10.000000 VESTS"
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> withdraw_vesting(
      const string& from,
      const serializer_wrapper<hive::protocol::asset>& vesting_shares,
      bool broadcast = false );

    /**
      * Set up a vesting withdraw route. When vesting shares are withdrawn, they will be routed to these accounts
      * based on the specified weights.
      *
      * @param from The account the VESTS are withdrawn from.
      * @param to   The account receiving either VESTS or HIVE.
      * @param percent The percent of the withdraw to go to the 'to' account. This is denoted in hundreths of a percent.
      *    i.e. 100 is 1% and 10000 is 100%. This value must be between 1 and 100000
      * @param auto_vest Set to true if the from account should receive the VESTS as VESTS, or false if it should receive
      *    them as HIVE.
      * @param broadcast true if you wish to broadcast the transaction.
      */
    serializer_wrapper<annotated_signed_transaction> set_withdraw_vesting_route(
      const string& from,
      const string& to,
      uint16_t percent,
      bool auto_vest,
      bool broadcast = false );

    /**
      *  This method will convert HBD to HIVE at the current_median_history price HIVE_CONVERSION_DELAY
      *  (3.5 days) from the time it is executed. This method depends upon there being a valid price feed.
      *
      *  @param from The account requesting conversion of its HBD i.e. "1.000 HBD"
      *  @param amount The amount of HBD to convert
      *  @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> convert_hbd(
      const string& from,
      const serializer_wrapper<hive::protocol::asset>& amount,
      bool broadcast = false );

    /**
      *  This method will convert HIVE to HBD at the market_median_history price plus HIVE_COLLATERALIZED_CONVERSION_FEE (5%)
      *  after HIVE_COLLATERALIZED_CONVERSION_DELAY (3.5 days) from the time it is executed. The caller gets HBD immediately
      *  calculated at current_min_history corrected with fee (as if conversion took place immediately, but only HIVE amount
      *  sliced by HIVE_CONVERSION_COLLATERAL_RATIO is calculated - that is, half the amount) and remainder from collateral
      *  amount is returned after actual conversion takes place. This method depends upon there being a valid price feed.
      *
      *  @param from The account requesting conversion of its HIVE i.e. "2.000 HIVE"
      *  @param amount The amount of HIVE collateral
      *  @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> convert_hive_with_collateral(
      const string& from,
      const serializer_wrapper<hive::protocol::asset>& collateral_amount,
      bool broadcast = false );

    /**
      *  Calculates amount of HIVE collateral to be passed to convert_hive_with_collateral in order to immediately get
      *  given hbd_amount_to_get in HBD. Note that there is no guarantee to get given HBD - when actual transaction
      *  takes place price might be different than during estimation.
      */
    serializer_wrapper<hive::protocol::asset> estimate_hive_collateral( const serializer_wrapper<hive::protocol::asset>& hbd_amount_to_get );

    /**
      * A witness can public a price feed for the HIVE:HBD market. The median price feed is used
      * to process conversion requests between HBD and HIVE.
      *
      * @param witness The witness publishing the price feed
      * @param exchange_rate The desired exchange rate
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> publish_feed(
      const string& witness,
      const serializer_wrapper<price>& exchange_rate,
      bool broadcast );

    /** Signs a transaction.
      *
      * Given a fully-formed transaction that is only lacking signatures, this signs
      * the transaction with the necessary keys and optionally broadcasts the transaction
      * @param tx the unsigned transaction
      * @param broadcast true if you wish to broadcast the transaction
      * @return the signed version of the transaction
      */
    serializer_wrapper<annotated_signed_transaction> sign_transaction(
      const serializer_wrapper<annotated_signed_transaction>& tx,
      bool broadcast = false);

    /** Returns an uninitialized object representing a given blockchain operation.
      *
      * This returns a default-initialized object of the given type; it can be used
      * during early development of the wallet when we don't yet have custom commands for
      * creating all of the operations the blockchain supports.
      *
      * Any operation the blockchain supports can be created using the transaction builder's
      * \c add_operation_to_builder_transaction() , but to do that from the CLI you need to
      * know what the JSON form of the operation looks like.  This will give you a template
      * you can fill in.  It's better than nothing.
      *
      * @param operation_type the type of operation to return, must be one of the
      *                       operations defined in `hive/chain/operations.hpp`
      *                       (e.g., "global_parameters_update_operation")
      * @return a default-constructed operation of the given type
      */
    serializer_wrapper<operation> get_prototype_operation(const string& operation_type);

    /**
      * Gets the current order book for HIVE:HBD
      *
      * @param limit Maximum number of orders to return for bids and asks. Max is 1000.
      */
    serializer_wrapper<wallet_bridge_api::get_order_book_return> get_order_book( uint32_t limit = 1000 );
    serializer_wrapper<vector< database_api::api_limit_order_object >> get_open_orders( const string& accountname );

    /**
      *  Creates a limit order at the price amount_to_sell / min_to_receive and will deduct amount_to_sell from account
      *
      *  @param owner The name of the account creating the order
      *  @param order_id is a unique identifier assigned by the creator of the order, it can be reused after the order has been filled
      *  @param amount_to_sell The amount of either HBD or HIVE you wish to sell
      *  @param min_to_receive The amount of the other asset you will receive at a minimum
      *  @param fill_or_kill true if you want the order to be killed if it cannot immediately be filled
      *  @param expiration the time the order should expire if it has not been filled
      *  @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> create_order(
      const string& owner,
      uint32_t order_id,
      const serializer_wrapper<hive::protocol::asset>& amount_to_sell,
      const serializer_wrapper<hive::protocol::asset>& min_to_receive,
      bool fill_or_kill,
      uint32_t expiration,
      bool broadcast );

    /**
      * Cancel an order created with create_order
      *
      * @param owner The name of the account owning the order to cancel_order
      * @param orderid The unique identifier assigned to the order by its creator
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> cancel_order(
      const string& owner,
      uint32_t orderid,
      bool broadcast );

    /**
      *  Post or update a comment.
      *
      *  @param author the name of the account authoring the comment
      *  @param permlink the accountwide unique permlink for the comment
      *  @param parent_author can be null if this is a top level comment
      *  @param parent_permlink becomes category if parent_author is ""
      *  @param title the title of the comment
      *  @param body the body of the comment
      *  @param json the json metadata of the comment
      *  @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> post_comment(
      const string& author,
      const string& permlink,
      const string& parent_author,
      const string& parent_permlink,
      const string& title,
      const string& body,
      const string& json,
      bool broadcast );

    /**
      * Vote on a comment to be paid HIVE
      *
      * @param voter The account voting
      * @param author The author of the comment to be voted on
      * @param permlink The permlink of the comment to be voted on. (author, permlink) is a unique pair
      * @param weight The weight [-100,100] of the vote
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> vote(
      const string& voter,
      const string& author,
      const string& permlink,
      int16_t weight,
      bool broadcast );

    /**
      * Sets the amount of time in the future until a transaction expires.
      */
    void set_transaction_expiration(uint32_t seconds);

    /**
      * Create an account recovery request as a recover account. The syntax for this command contains a serialized authority object
      * so there is an example below on how to pass in the authority.
      *
      * request_account_recovery "your_account" "account_to_recover" {"weight_threshold": 1,"account_auths": [], "key_auths": [["new_public_key",1]]} true
      *
      * @param recovery_account The name of your account
      * @param account_to_recover The name of the account you are trying to recover
      * @param new_authority The new owner authority for the recovered account. This should be given to you by the holder of the compromised or lost account.
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> request_account_recovery(
      const string& recovery_account,
      const string& account_to_recover,
      authority new_authority,
      bool broadcast );

    /**
      * Recover your account using a recovery request created by your recovery account. The syntax for this commain contains a serialized
      * authority object, so there is an example below on how to pass in the authority.
      *
      * recover_account "your_account" {"weight_threshold": 1,"account_auths": [], "key_auths": [["old_public_key",1]]} {"weight_threshold": 1,"account_auths": [], "key_auths": [["new_public_key",1]]} true
      *
      * @param account_to_recover The name of your account
      * @param recent_authority A recent owner authority on your account
      * @param new_authority The new authority that your recovery account used in the account recover request.
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> recover_account(
      const string& account_to_recover,
      authority recent_authority,
      authority new_authority,
      bool broadcast );

    /**
      * Change your recovery account after a 30 day delay.
      *
      * @param owner The name of your account
      * @param new_recovery_account The name of the recovery account you wish to have
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> change_recovery_account(
      const string& owner,
      const string& new_recovery_account,
      bool broadcast );

    vector< database_api::api_owner_authority_history_object > get_owner_history( const string& account )const;

    /**
      *  Account operations have sequence numbers from 0 to N where N is the most recent operation. This method
      *  returns operations in the range [from-limit, from]
      *
      *  @param account - account whose history will be returned
      *  @param from - the absolute sequence number, -1 means most recent, limit is the number of operations before from.
      *  @param limit - the maximum number of items that can be queried (0 to 1000], must be less than from
      */
    serializer_wrapper<map< uint32_t, account_history::api_operation_object >> get_account_history( const string& account, uint32_t from, uint32_t limit );


    FC_TODO(Supplement API argument description)
    /**
      *  Marks one account as following another account.  Requires the posting authority of the follower.
      *
      *  @param follower
      *  @param following
      *  @param what - a set of things to follow: posts, comments, votes, ignore
      *  @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> follow( const string& follower, const string& following, set<string> what, bool broadcast );

    /**
      * Checks memos against private keys on account and imported in wallet
      */
    void check_memo( const string& memo, const database_api::api_account_object& account )const;

    /**
      *  Returns the encrypted memo if memo starts with '#' otherwise returns memo
      */
    string get_encrypted_memo( const string& from, const string& to, const string& memo );

    // helper for above
    string get_encrypted_memo_using_keys( const public_key_type& from_key, const public_key_type& to_key, string memo ) const;

    /**
      * Returns the decrypted memo if possible given wallet's known private keys
      */
    string decrypt_memo( string memo );

    serializer_wrapper<annotated_signed_transaction> decline_voting_rights( const string& account, bool decline, bool broadcast );

    serializer_wrapper<annotated_signed_transaction> claim_reward_balance(
      const string& account,
      const serializer_wrapper<hive::protocol::asset>& reward_hive,
      const serializer_wrapper<hive::protocol::asset>& reward_hbd,
      const serializer_wrapper<hive::protocol::asset>& reward_vests,
      bool broadcast );

    /**
      * Create worker proposal
      * @param creator    - the account that creates the proposal,
      * @param receiver   - the account that will be funded,
      * @param start_date - start date of proposal,
      * @param end_date   - end date of proposal,
      * @param daily_pay  - the amount of HBD that is being requested to be paid out daily,
      * @param subject    - briefly description of proposal of its title,
      * @param permlink   - permlink of the post for the proposal.
      */
    serializer_wrapper<annotated_signed_transaction> create_proposal( const account_name_type& creator,
                  const account_name_type& receiver,
                  time_point_sec start_date,
                  time_point_sec end_date,
                  const serializer_wrapper<hive::protocol::asset>& daily_pay,
                  string subject,
                  string permlink,
                  bool broadcast );
    /**
      * Update worker proposal
      * @param proposal_id - the id of the proposal to update,
      * @param creator     - the account that created the proposal,
      * @param daily_pay   - new amount of HBD to be requested to be paid out daily,
      * @param subject     - new description of the proposal,
      * @param permlink    - new permlink of the post for the proposal.
      * @param end_date    - new end_date of the proposal.
      */
    serializer_wrapper<annotated_signed_transaction> update_proposal(
                  int64_t proposal_id,
                  const account_name_type& creator,
                  const serializer_wrapper<hive::protocol::asset>& daily_pay,
                  string subject,
                  string permlink,
                  optional<time_point_sec> end_date,
                  bool broadcast );
    /**
      * Update existing worker proposal(s)
      * @param voter     - the account that votes,
      * @param proposals - array with proposal ids,
      * @param approve   - set if proposal(s) should be approved or not.
      */
    serializer_wrapper<annotated_signed_transaction> update_proposal_votes(const account_name_type& voter,
                                              const flat_set< int64_t >& proposals,
                                              bool approve,
                                              bool broadcast );
    /**
      * List proposals
      * @param start      - starting value for querying results,
      * @param limit      - query limit,
      * @param order_by   - name a field for sorting operation,
      * @param order_type - set print order (ascending, descending)
      * @param status     - list only results with given status (all, inactive, active, expired, votable),
      */
    serializer_wrapper<vector< database_api::api_proposal_object >> list_proposals( fc::variant start,
                                      uint32_t limit,
                                      database_api::sort_order_type order_by,
                                      database_api::order_direction_type order_type = database_api::descending,
                                      database_api::proposal_status status = database_api::all );

    /**
      * Find proposal with given id
      * @param _ids - array with ids of wanted proposals to be founded.
      */
    serializer_wrapper<vector< database_api::api_proposal_object >> find_proposals( vector< database_api::api_id_type > proposal_ids );

    /**
      * List proposal votes
      * @param start      - starting value for querying results,
      * @param limit      - query limit,
      * @param order_by   - name a field for sorting operation,
      * @param order_type - set print order (ascending, descending)
      * @param status     - list only results with given status (all, inactive, active, expired, votable),
      */
    serializer_wrapper<vector< database_api::api_proposal_vote_object >> list_proposal_votes( fc::variant start,
                                              uint32_t limit,
                                              database_api::sort_order_type order_by,
                                              database_api::order_direction_type order_type = database_api::descending,
                                              database_api::proposal_status status = database_api::all );

    /**
      * Remove given proposal
      * @param deleter   - authorized account,
      * @param ids       - proposal ids to be removed.
      */
    serializer_wrapper<annotated_signed_transaction> remove_proposal( const account_name_type& deleter,
                                            const flat_set< int64_t >& ids,
                                            bool broadcast );

    /**
      * Creates a recurrent transfer of funds from one account to another. HIVE and HBD can be transferred.
      *
      * @param from The account the funds are coming from
      * @param to The account from which the funds are going to
      * @param amount The funds being transferred. i.e. "100.000 HIVE"
      * @param memo A memo for the transaction, encrypted with the to account's public memo key
      * @param recurrence how often the transfer should be executed in hours. eg: 24
      * @param executions how many times should the recurrent transfer be executed
      * @param broadcast true if you wish to broadcast the transaction
      */
    serializer_wrapper<annotated_signed_transaction> recurrent_transfer(
            const account_name_type& from,
            const account_name_type& to,
            const serializer_wrapper<hive::protocol::asset>& amount,
            const string& memo,
            uint16_t recurrence,
            uint16_t executions,
            bool broadcast );

  /**
      * Finds a recurrent transfer

      * @param from The account from which the funds are coming from
      */
  serializer_wrapper<vector< database_api::api_recurrent_transfer_object >> find_recurrent_transfers(
          const account_name_type& from );

    std::map<string,std::function<string(fc::variant,const fc::variants&)>> get_result_formatters() const;

    fc::signal<void(bool)> lock_changed;
    std::shared_ptr<detail::wallet_api_impl> my;
    void encrypt_keys();
};

struct plain_keys {
  fc::sha512                  checksum;
  map<public_key_type,string> keys;
};

} }

FC_REFLECT( hive::wallet::wallet_data,
        (cipher_keys)
        (ws_server)
        (ws_user)
        (ws_password)
        )

FC_REFLECT( hive::wallet::brain_key_info, (brain_priv_key)(wif_priv_key) (pub_key))

FC_REFLECT( hive::wallet::plain_keys, (checksum)(keys) )

FC_REFLECT_ENUM( hive::wallet::authority_type, (owner)(active)(posting) )

FC_API( hive::wallet::wallet_api,
      /// wallet api
      (help)(gethelp)
      (about)(is_new)(is_locked)(lock)(unlock)(set_password)
      (load_wallet_file)(save_wallet_file)

      /// key api
      (import_key)
      (suggest_brain_key)
      (list_keys)
      (get_private_key)
      (get_private_key_from_password)
      (normalize_brain_key)

      /// query api
      (info)
      (list_my_accounts)
      (list_accounts)
      (list_witnesses)
      (get_witness)
      (get_account)
      (get_accounts)
      (get_block)
      (get_ops_in_block)
      (get_feed_history)
      (get_conversion_requests)
      (get_collateralized_conversion_requests)
      (get_account_history)
      (get_withdraw_routes)
      (find_recurrent_transfers)

      /// transaction api
      (claim_account_creation)
      (claim_account_creation_nonblocking)
      (create_account)
      (create_account_with_keys)
      (create_funded_account_with_keys)
      (create_account_delegated)
      (create_account_with_keys_delegated)
      (update_account)
      (update_account_auth_key)
      (update_account_auth_account)
      (update_account_auth_threshold)
      (update_account_meta)
      (update_account_memo_key)
      (delegate_vesting_shares)
      (delegate_vesting_shares_nonblocking)
      (delegate_vesting_shares_and_transfer)
      (delegate_vesting_shares_and_transfer_nonblocking)
      (update_witness)
      (set_voting_proxy)
      (vote_for_witness)
      (follow)
      (transfer)
      (transfer_nonblocking)
      (escrow_transfer)
      (escrow_approve)
      (escrow_dispute)
      (escrow_release)
      (transfer_to_vesting)
      (transfer_to_vesting_nonblocking)
      (withdraw_vesting)
      (set_withdraw_vesting_route)
      (convert_hbd)
      (convert_hive_with_collateral)
      (estimate_hive_collateral)
      (publish_feed)
      (get_order_book)
      (get_open_orders)
      (create_order)
      (cancel_order)
      (post_comment)
      (vote)
      (set_transaction_expiration)
      (request_account_recovery)
      (recover_account)
      (change_recovery_account)
      (get_owner_history)
      (transfer_to_savings)
      (transfer_from_savings)
      (cancel_transfer_from_savings)
      (get_encrypted_memo)
      (decrypt_memo)
      (decline_voting_rights)
      (claim_reward_balance)
      (recurrent_transfer)

      /// helper api
      (get_prototype_operation)
      (serialize_transaction)
      (sign_transaction)

      (get_active_witnesses)
      (get_transaction)

      ///worker proposal api
      (create_proposal)
      (update_proposal)
      (update_proposal_votes)
      (list_proposals)
      (find_proposals)
      (list_proposal_votes)
      (remove_proposal)
    )

FC_REFLECT( hive::wallet::memo_data, (from)(to)(nonce)(check)(encrypted) )

namespace fc {

  template<typename T>
  inline void to_variant( const hive::wallet::serializer_wrapper<T>& a, fc::variant& var )
  {
    //Compatibility with older shape of asset
    bool old_legacy_enabled = hive::protocol::dynamic_serializer::legacy_enabled;
    hive::protocol::dynamic_serializer::legacy_enabled = true;

    to_variant( a.value, var );

    hive::protocol::dynamic_serializer::legacy_enabled = old_legacy_enabled;
  }

  template<typename T>
  inline void from_variant( const fc::variant& var, hive::wallet::serializer_wrapper<T>& a )
  {
    //Compatibility with older shape of asset
    bool old_legacy_enabled = hive::protocol::dynamic_serializer::legacy_enabled;
    hive::protocol::dynamic_serializer::legacy_enabled = true;

    from_variant( var, a.value );

    hive::protocol::dynamic_serializer::legacy_enabled = old_legacy_enabled;
  }

} // fc

FC_REFLECT_TEMPLATE( (typename T), hive::wallet::serializer_wrapper<T>, (value) )
