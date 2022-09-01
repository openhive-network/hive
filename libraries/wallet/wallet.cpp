#include <hive/chain/hive_fwd.hpp>

#include <hive/utilities/git_revision.hpp>
#include <hive/utilities/key_conversion.hpp>
#include <hive/utilities/words.hpp>

#include <hive/protocol/base.hpp>
#include <hive/protocol/sps_operations.hpp>
#include <hive/wallet/wallet.hpp>
#include <hive/wallet/api_documentation.hpp>
#include <hive/wallet/reflect_util.hpp>

#include <hive/plugins/follow/follow_operations.hpp>
#include <hive/plugins/wallet_bridge_api/wallet_bridge_api_plugin.hpp>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <list>

#include <boost/version.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/range/algorithm/unique.hpp>
#include <boost/range/algorithm/sort.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include <fc/container/deque.hpp>
#include <fc/git_revision.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/io/stdio.hpp>
#include <fc/macros.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/crypto/rand.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/smart_ref_impl.hpp>

#ifndef WIN32
# include <sys/types.h>
# include <sys/stat.h>
#endif

#define BRAIN_KEY_WORD_COUNT 16

constexpr bool LOCK = false;  //DECLARE_API addes lock argument to all wallet_bridge_api methods. Default value is false, so we need to pass it here.

namespace hive { namespace wallet {

namespace detail {

template<class T>
optional<T> maybe_id( const string& name_or_id )
{
  if( std::isdigit( name_or_id.front() ) )
  {
    try
    {
      return fc::variant(name_or_id).as<T>();
    }
    catch (const fc::exception&)
    {
    }
  }
  return optional<T>();
}

string pubkey_to_shorthash( const public_key_type& key )
{
  uint32_t x = fc::sha256::hash(key)._hash[0];
  static const char hd[] = "0123456789abcdef";
  string result;

  result += hd[(x >> 0x1c) & 0x0f];
  result += hd[(x >> 0x18) & 0x0f];
  result += hd[(x >> 0x14) & 0x0f];
  result += hd[(x >> 0x10) & 0x0f];
  result += hd[(x >> 0x0c) & 0x0f];
  result += hd[(x >> 0x08) & 0x0f];
  result += hd[(x >> 0x04) & 0x0f];
  result += hd[(x        ) & 0x0f];

  return result;
}


fc::ecc::private_key derive_private_key( const std::string& prefix_string,
                            int sequence_number )
{
  std::string sequence_string = std::to_string(sequence_number);
  fc::sha512 h = fc::sha512::hash(prefix_string + " " + sequence_string);
  fc::ecc::private_key derived_key = fc::ecc::private_key::regenerate(fc::sha256::hash(h));
  return derived_key;
}

string normalize_brain_key( string s )
{
  size_t i = 0, n = s.length();
  std::string result;
  char c;
  result.reserve( n );

  bool preceded_by_whitespace = false;
  bool non_empty = false;
  while( i < n )
  {
    c = s[i++];
    switch( c )
    {
    case ' ':  case '\t': case '\r': case '\n': case '\v': case '\f':
      preceded_by_whitespace = true;
      continue;

    case 'a': c = 'A'; break;
    case 'b': c = 'B'; break;
    case 'c': c = 'C'; break;
    case 'd': c = 'D'; break;
    case 'e': c = 'E'; break;
    case 'f': c = 'F'; break;
    case 'g': c = 'G'; break;
    case 'h': c = 'H'; break;
    case 'i': c = 'I'; break;
    case 'j': c = 'J'; break;
    case 'k': c = 'K'; break;
    case 'l': c = 'L'; break;
    case 'm': c = 'M'; break;
    case 'n': c = 'N'; break;
    case 'o': c = 'O'; break;
    case 'p': c = 'P'; break;
    case 'q': c = 'Q'; break;
    case 'r': c = 'R'; break;
    case 's': c = 'S'; break;
    case 't': c = 'T'; break;
    case 'u': c = 'U'; break;
    case 'v': c = 'V'; break;
    case 'w': c = 'W'; break;
    case 'x': c = 'X'; break;
    case 'y': c = 'Y'; break;
    case 'z': c = 'Z'; break;

    default:
      break;
    }
    if( preceded_by_whitespace && non_empty )
      result.push_back(' ');
    result.push_back(c);
    preceded_by_whitespace = false;
    non_empty = true;
  }
  return result;
}

struct op_prototype_visitor
{
  typedef void result_type;

  int t = 0;
  flat_map< std::string, operation >& name2op;

  op_prototype_visitor(
    int _t,
    flat_map< std::string, operation >& _prototype_ops
    ):t(_t), name2op(_prototype_ops) {}

  template<typename Type>
  result_type operator()( const Type& op )const
  {
    string name = fc::get_typename<Type>::name();
    size_t p = name.rfind(':');
    if( p != string::npos )
      name = name.substr( p+1 );
    name2op[ name ] = Type();
  }
};

class wallet_api_impl
{
  public:
    api_documentation method_documentation;
  private:
    void enable_umask_protection()
    {
#ifdef __unix__
      _old_umask = umask( S_IRWXG | S_IRWXO );
#endif
    }

    void disable_umask_protection()
    {
#ifdef __unix__
      umask( _old_umask );
#endif
    }

    void init_prototype_ops()
    {
      operation op;
      for( int t=0; t<op.count(); t++ )
      {
        op.set_which( t );
        op.visit( op_prototype_visitor(t, _prototype_ops) );
      }
      return;
    }

public:
  wallet_api& self;
  wallet_api_impl( wallet_api& s, const wallet_data& initial_data, const chain_id_type& hive_chain_id, const fc::api< hive::plugins::wallet_bridge_api::wallet_bridge_api >& remote_api )
    : self( s ), _remote_wallet_bridge_api(remote_api)
  {
    init_prototype_ops();

    _wallet.ws_server = initial_data.ws_server;
    _hive_chain_id = hive_chain_id;
  }

  virtual ~wallet_api_impl()
  {}

  void encrypt_keys()
  {
    if( !is_locked() )
    {
      plain_keys data;
      data.keys = _keys;
      data.checksum = _checksum;
      auto plain_txt = fc::raw::pack_to_vector(data);
      _wallet.cipher_keys = fc::aes_encrypt( data.checksum, plain_txt );
    }
  }

  bool copy_wallet_file( const string& destination_filename )
  {
    fc::path src_path = get_wallet_filename();
    if( !fc::exists( src_path ) )
      return false;
    fc::path dest_path = destination_filename + _wallet_filename_extension;
    int suffix = 0;
    while( fc::exists(dest_path) )
    {
      ++suffix;
      dest_path = destination_filename + "-" + std::to_string( suffix ) + _wallet_filename_extension;
    }
    wlog( "backing up wallet ${src} to ${dest}",
        ("src", src_path)
        ("dest", dest_path) );

    fc::path dest_parent = fc::absolute(dest_path).parent_path();
    try
    {
      enable_umask_protection();
      if( !fc::exists( dest_parent ) )
        fc::create_directories( dest_parent );
      fc::copy( src_path, dest_path );
      disable_umask_protection();
    }
    catch(...)
    {
      disable_umask_protection();
      throw;
    }
    return true;
  }

  bool is_locked()const
  {
    return _checksum == fc::sha512();
  }

  variant info() const
  {
    auto dynamic_props = _remote_wallet_bridge_api->get_dynamic_global_properties({}, LOCK);
    fc::mutable_variant_object result;
    result["witness_majority_version"] = fc::string( _remote_wallet_bridge_api->get_witness_schedule({}, LOCK).majority_version);
    result["hardfork_version"] = fc::string( _remote_wallet_bridge_api->get_hardfork_version({}, LOCK) );
    result["head_block_num"] = dynamic_props.head_block_number;
    result["head_block_id"] = dynamic_props.head_block_id;
    result["head_block_age"] = fc::get_approximate_relative_time_string(dynamic_props.time,
                                                  time_point_sec(time_point::now()),
                                                  " old");
    result["participation"] = (100*dynamic_props.recent_slots_filled.popcount()) / 128.0;
    result["median_hbd_price"] = _remote_wallet_bridge_api->get_current_median_history_price({}, LOCK);
    result["account_creation_fee"] = _remote_wallet_bridge_api->get_chain_properties({}, LOCK).account_creation_fee;
    result["post_reward_fund"] = _remote_wallet_bridge_api->get_reward_fund({HIVE_POST_REWARD_FUND_NAME}, LOCK );
    return result;
  }

  variant_object about() const
  {
    string client_version( hive::utilities::git_revision_description );
    const size_t pos = client_version.find( '/' );
    if( pos != string::npos && client_version.size() > pos )
      client_version = client_version.substr( pos + 1 );

    fc::mutable_variant_object result;
    result["blockchain_version"]       = HIVE_BLOCKCHAIN_VERSION;
    result["client_version"]           = client_version;
    result["hive_revision"]            = hive::utilities::git_revision_sha;
    result["hive_revision_age"]        = fc::get_approximate_relative_time_string( fc::time_point_sec( hive::utilities::git_revision_unix_timestamp ) );
    result["fc_revision"]              = fc::git_revision_sha;
    result["fc_revision_age"]          = fc::get_approximate_relative_time_string( fc::time_point_sec( fc::git_revision_unix_timestamp ) );
    result["compile_date"]             = "compiled on " __DATE__ " at " __TIME__;
    result["boost_version"]            = boost::replace_all_copy(std::string(BOOST_LIB_VERSION), "_", ".");
    result["openssl_version"]          = OPENSSL_VERSION_TEXT;

    std::string bitness = boost::lexical_cast<std::string>(8 * sizeof(int*)) + "-bit";
#if defined(__APPLE__)
    std::string os = "osx";
#elif defined(__linux__)
    std::string os = "linux";
#elif defined(_MSC_VER)
    std::string os = "win32";
#else
    std::string os = "other";
#endif
    result["build"] = os + " " + bitness;

    try
    {
      auto v = _remote_wallet_bridge_api->get_version({}, LOCK);
      result["server_blockchain_version"] = v.blockchain_version;
      result["server_hive_revision"] = v.hive_revision;
      result["server_fc_revision"] = v.fc_revision;
    }
    catch( fc::exception& )
    {
      result["server"] = "could not retrieve server version information";
    }

    return result;
  }

  database_api::api_account_object get_account( const string& account_name ) const
  {
    auto account = _remote_wallet_bridge_api->get_account( { account_name }, LOCK );
    FC_ASSERT( account.valid(), "Unknown account" );
    return *account;
  }

  string get_wallet_filename() const { return _wallet_filename; }

  optional<fc::ecc::private_key>  try_get_private_key(const public_key_type& id)const
  {
    auto it = _keys.find(id);
    if( it != _keys.end() )
      return wif_to_key( it->second );
    return optional<fc::ecc::private_key>();
  }

  fc::ecc::private_key              get_private_key(const public_key_type& id)const
  {
    auto has_key = try_get_private_key( id );
    FC_ASSERT( has_key );
    return *has_key;
  }


  fc::ecc::private_key get_private_key_for_account(const database_api::api_account_object& account)const
  {
    vector<public_key_type> active_keys = account.active.get_keys();
    if( active_keys.size() != 1 )
      FC_THROW("Expecting a simple authority with one active key");
    return get_private_key(active_keys.front());
  }

  // imports the private key into the wallet, and associate it in some way (?) with the
  // given account name.
  // @returns true if the key matches a current active/owner/memo key for the named
  //          account, false otherwise (but it is stored either way)
  bool import_key(const string& wif_key)
  {
    fc::optional<fc::ecc::private_key> optional_private_key = wif_to_key(wif_key);
    if( !optional_private_key )
      FC_THROW("Invalid private key");
    hive::chain::public_key_type wif_pub_key = optional_private_key->get_public_key();

    _keys[wif_pub_key] = wif_key;
    return true;
  }

  bool load_wallet_file(string wallet_filename = "")
  {
    // TODO:  Merge imported wallet with existing wallet,
    //        instead of replacing it
    if( wallet_filename == "" )
      wallet_filename = _wallet_filename;

    if( ! fc::exists( wallet_filename ) )
      return false;

    _wallet = fc::json::from_file( wallet_filename ).as< wallet_data >();

    return true;
  }

  void save_wallet_file(string wallet_filename = "")
  {
    //
    // Serialize in memory, then save to disk
    //
    // This approach lessens the risk of a partially written wallet
    // if exceptions are thrown in serialization
    //

    encrypt_keys();

    if( wallet_filename == "" )
      wallet_filename = _wallet_filename;

    wlog( "saving wallet to file ${fn}", ("fn", wallet_filename) );

    string data = fc::json::to_pretty_string( _wallet );
    try
    {
      enable_umask_protection();
      //
      // Parentheses on the following declaration fails to compile,
      // due to the Most Vexing Parse.  Thanks, C++
      //
      // http://en.wikipedia.org/wiki/Most_vexing_parse
      //
      fc::ofstream outfile{ fc::path( wallet_filename ) };
      outfile.write( data.c_str(), data.length() );
      outfile.flush();
      outfile.close();
      disable_umask_protection();
    }
    catch(...)
    {
      disable_umask_protection();
      throw;
    }
  }

  // This function generates derived keys starting with index 0 and keeps incrementing
  // the index until it finds a key that isn't registered in the block chain.  To be
  // safer, it continues checking for a few more keys to make sure there wasn't a short gap
  // caused by a failed registration or the like.
  int find_first_unused_derived_key_index(const fc::ecc::private_key& parent_key)
  {
    int first_unused_index = 0;
    int number_of_consecutive_unused_keys = 0;
    for( int key_index = 0; ; ++key_index )
    {
      fc::ecc::private_key derived_private_key = derive_private_key(key_to_wif(parent_key), key_index);
      hive::chain::public_key_type derived_public_key = derived_private_key.get_public_key();
      if( _keys.find(derived_public_key) == _keys.end() )
      {
        if( number_of_consecutive_unused_keys )
        {
          ++number_of_consecutive_unused_keys;
          if( number_of_consecutive_unused_keys > 5 )
            return first_unused_index;
        }
        else
        {
          first_unused_index = key_index;
          number_of_consecutive_unused_keys = 1;
        }
      }
      else
      {
        // key_index is used
        first_unused_index = 0;
        number_of_consecutive_unused_keys = 0;
      }
    }
  }

  annotated_signed_transaction create_account_with_private_key(const fc::ecc::private_key& owner_privkey,
                                    const string& account_name,
                                    const string& creator_account_name,
                                    bool broadcast = false,
                                    bool save_wallet = true)
  { try {
      int active_key_index = find_first_unused_derived_key_index(owner_privkey);
      fc::ecc::private_key active_privkey = derive_private_key( key_to_wif(owner_privkey), active_key_index);

      int memo_key_index = find_first_unused_derived_key_index(active_privkey);
      fc::ecc::private_key memo_privkey = derive_private_key( key_to_wif(active_privkey), memo_key_index);

      hive::chain::public_key_type owner_pubkey = owner_privkey.get_public_key();
      hive::chain::public_key_type active_pubkey = active_privkey.get_public_key();
      hive::chain::public_key_type memo_pubkey = memo_privkey.get_public_key();

      account_create_operation account_create_op;

      account_create_op.creator = creator_account_name;
      account_create_op.new_account_name = account_name;
      account_create_op.fee = _remote_wallet_bridge_api->get_chain_properties({}, LOCK).account_creation_fee;
      account_create_op.owner = authority(1, owner_pubkey, 1);
      account_create_op.active = authority(1, active_pubkey, 1);
      account_create_op.memo_key = memo_pubkey;

      signed_transaction tx;

      tx.operations.push_back( account_create_op );
      tx.validate();

      if( save_wallet )
        save_wallet_file();
      if( broadcast )
      {
        //_remote_wallet_bridge_api->broadcast_transaction( tx );
        auto result = _remote_wallet_bridge_api->broadcast_transaction_synchronous({variant(tx)}, LOCK);
        FC_UNUSED(result);
      }
      return tx;
  } FC_CAPTURE_AND_RETHROW( (account_name)(creator_account_name)(broadcast) ) }

  annotated_signed_transaction set_voting_proxy(const string& account_to_modify, const string& proxy, bool broadcast /* = false */)
  { try {
    account_witness_proxy_operation op;
    op.account = account_to_modify;
    op.proxy = proxy;

    signed_transaction tx;
    tx.operations.push_back( op );
    tx.validate();

    return sign_transaction( tx, broadcast );
  } FC_CAPTURE_AND_RETHROW( (account_to_modify)(proxy)(broadcast) ) }

  optional< database_api::api_witness_object > get_witness( const string& owner_account )
  {
    return _remote_wallet_bridge_api->get_witness( {owner_account}, LOCK );
  }

  /// Common body for claim_account_creation and claim_account_creation_nonblocking
  condenser_api::legacy_signed_transaction build_claim_account_creation(const string& creator, const condenser_api::legacy_asset& fee,
    const std::function<condenser_api::legacy_signed_transaction(signed_transaction)>& tx_signer);

  void set_transaction_expiration( uint32_t tx_expiration_seconds )
  {
    FC_ASSERT( tx_expiration_seconds < HIVE_MAX_TIME_UNTIL_EXPIRATION );
    _tx_expiration_seconds = tx_expiration_seconds;
  }

  // sets the expiration time and reference block
  void initialize_transaction_header(transaction& tx)
  {
    auto dyn_props = _remote_api->get_dynamic_global_properties();
    tx.set_reference_block( dyn_props.head_block_id );
    tx.set_expiration( dyn_props.time + fc::seconds(_tx_expiration_seconds) );
  }

  // if the user rapidly sends two identical transactions (within the same block),
  // the second one will fail because it will have the same transaction id.  Waiting
  // a few seconds before sending the second transaction will allow it to succeed,
  // because the second transaction will be assigned a later expiration time.
  // This isn't a good solution for scripts, so we provide this method which
  // adds a do-nothing custom operation to the transaction which contains a
  // random 64-bit number, which will change the transaction's hash to prevent
  // collisions.
  void make_transaction_unique(transaction& tx, const std::string& auth)
  {
    initialize_transaction_header(tx);
    if( _remote_api->is_known_transaction( tx.id() ) )
    {
      // create a custom operation with a random 64-bit integer which will give this
      // transaction a new id
      custom_operation custom_op;
      custom_op.data.resize(8);
      fc::rand_bytes(custom_op.data.data(), custom_op.data.size());
      custom_op.required_auths.insert(auth);
      tx.operations.push_back(custom_op);
      tx.validate();
    }
  }

  condenser_api::legacy_signed_transaction sign_transaction(
    signed_transaction tx,
    bool broadcast = false )
  {
    return sign_and_broadcast_transaction(std::move(tx), broadcast, true);
  }

  condenser_api::legacy_signed_transaction sign_and_broadcast_transaction(signed_transaction tx, bool broadcast, bool blocking)
  {
    static const authority null_auth( 1, public_key_type(), 0 );
    flat_set< account_name_type >   req_active_approvals;
    flat_set< account_name_type >   req_owner_approvals;
    flat_set< account_name_type >   req_posting_approvals;
    vector< authority >  other_auths;

    tx.get_required_authorities( req_active_approvals, req_owner_approvals, req_posting_approvals, other_auths );

    for( const auto& auth : other_auths )
      for( const auto& a : auth.account_auths )
        req_active_approvals.insert(a.first);

    // std::merge lets us de-duplicate account_id's that occur in both
    //   sets, and dump them into a vector (as required by remote_db api)
    //   at the same time
    vector< account_name_type > v_approving_account_names;
    std::merge(req_active_approvals.begin(), req_active_approvals.end(),
            req_owner_approvals.begin() , req_owner_approvals.end(),
            std::back_inserter( v_approving_account_names ) );

    for( const auto& a : req_posting_approvals )
      v_approving_account_names.push_back(a);

    /// TODO: fetch the accounts specified via other_auths as well.

    auto approving_account_objects = _remote_wallet_bridge_api->get_accounts({variant(v_approving_account_names)}, LOCK);

    /// TODO: recursively check one layer deeper in the authority tree for keys

    FC_ASSERT( approving_account_objects.size() == v_approving_account_names.size(), "", ("aco.size:", approving_account_objects.size())("acn",v_approving_account_names.size()) );

    flat_map< string, database_api::api_account_object > approving_account_lut;
    size_t i = 0;
    for( const auto& approving_acct : approving_account_objects )
    {
      approving_account_lut[ approving_acct.name ] =  approving_acct;
      i++;
    }

    auto get_account_from_lut = [&]( const std::string& name ) -> fc::optional< const database_api::api_account_object* >
    {
      fc::optional< const database_api::api_account_object* > result;
      auto it = approving_account_lut.find( name );
      if( it != approving_account_lut.end() )
      {
        result = &(it->second);
      }
      else
      {
        elog( "Tried to access authority for account ${a}.", ("a", name) );
        elog( "Is it possible you are using an account authority? Signing with an account authority is currently not supported." );
      }

      return result;
    };

    flat_set<public_key_type> approving_key_set;
    for( account_name_type& acct_name : req_active_approvals )
    {
      const auto it = approving_account_lut.find( acct_name );
      if( it == approving_account_lut.end() )
        continue;

      const database_api::api_account_object& acct = it->second;
      vector<public_key_type> v_approving_keys = acct.active.get_keys();
      wdump((v_approving_keys));
      for( const public_key_type& approving_key : v_approving_keys )
      {
        wdump((approving_key));
        approving_key_set.insert( approving_key );
      }
    }

    for( account_name_type& acct_name : req_posting_approvals )
    {
      const auto it = approving_account_lut.find( acct_name );
      if( it == approving_account_lut.end() )
        continue;

      const database_api::api_account_object& acct = it->second;
      vector<public_key_type> v_approving_keys = acct.posting.get_keys();
      wdump((v_approving_keys));
      for( const public_key_type& approving_key : v_approving_keys )
      {
        wdump((approving_key));
        approving_key_set.insert( approving_key );
      }
    }

    for( const account_name_type& acct_name : req_owner_approvals )
    {
      const auto it = approving_account_lut.find( acct_name );
      if( it == approving_account_lut.end() )
        continue;

      const database_api::api_account_object& acct = it->second;
      vector<public_key_type> v_approving_keys = acct.owner.get_keys();
      for( const public_key_type& approving_key : v_approving_keys )
      {
        wdump((approving_key));
        approving_key_set.insert( approving_key );
      }
    }
    for( const authority& a : other_auths )
    {
      for( const auto& k : a.key_auths )
      {
        wdump((k.first));
        approving_key_set.insert( k.first );
      }
    }

    auto dyn_props = _remote_wallet_bridge_api->get_dynamic_global_properties({},LOCK);
    tx.set_reference_block( dyn_props.head_block_id );
    tx.set_expiration( dyn_props.time + fc::seconds(_tx_expiration_seconds) );
    tx.signatures.clear();

    //idump((_keys));
    flat_set< public_key_type > available_keys;
    flat_map< public_key_type, fc::ecc::private_key > available_private_keys;
    for( const public_key_type& key : approving_key_set )
    {
      auto it = _keys.find(key);
      if( it != _keys.end() )
      {
        fc::optional<fc::ecc::private_key> privkey = wif_to_key( it->second );
        FC_ASSERT( privkey.valid(), "Malformed private key in _keys" );
        available_keys.insert(key);
        available_private_keys[key] = *privkey;
      }
    }

    auto minimal_signing_keys = tx.minimize_required_signatures(
      _hive_chain_id,
      available_keys,
      [&]( const string& account_name ) -> const authority&
      {
        auto maybe_account = get_account_from_lut( account_name );
        if( maybe_account.valid() )
          return (*maybe_account)->active;

        return null_auth;
      },
      [&]( const string& account_name ) -> const authority&
      {
        auto maybe_account = get_account_from_lut( account_name );
        if( maybe_account.valid() )
          return (*maybe_account)->owner;

        return null_auth;
      },
      [&]( const string& account_name ) -> const authority&
      {
        auto maybe_account = get_account_from_lut( account_name );
        if( maybe_account.valid() )
          return (*maybe_account)->posting;

        return null_auth;
      },
      HIVE_MAX_SIG_CHECK_DEPTH,
      HIVE_MAX_AUTHORITY_MEMBERSHIP,
      HIVE_MAX_SIG_CHECK_ACCOUNTS,
      fc::ecc::fc_canonical
      );

    for( const public_key_type& k : minimal_signing_keys )
    {
      auto it = available_private_keys.find(k);
      FC_ASSERT( it != available_private_keys.end() );
      tx.sign( it->second, _hive_chain_id, fc::ecc::fc_canonical );
    }

    if( broadcast )
    {
      try
      {
        auto result = _remote_wallet_bridge_api->broadcast_transaction_synchronous({variant(tx)}, LOCK);
        annotated_signed_transaction rtrx(tx);
        rtrx.block_num = result.block_num;
        rtrx.transaction_num = result.trx_num;
        return rtrx;
      }
      catch (const fc::exception& e)
      {
        elog("Caught exception while broadcasting tx ${id}:  ${e}", ("id", tx.id().str())("e", e.to_detail_string()) );
        throw;
      }
    }
    return tx;
  }

  std::map<string,std::function<string(fc::variant,const fc::variants&)>> get_result_formatters() const
  {
    std::map<string,std::function<string(fc::variant,const fc::variants&)> > m;
    m["help"] = [](variant result, const fc::variants& a)
    {
      return result.get_string();
    };

    m["gethelp"] = [](variant result, const fc::variants& a)
    {
      return result.get_string();
    };

    m["list_my_accounts"] = [](variant result, const fc::variants& a )
    {
      std::stringstream out;

      auto accounts = result.as<vector<database_api::api_account_object>>();
      asset total_hive;
      asset total_vest(0, VESTS_SYMBOL );
      asset total_hbd(0, HBD_SYMBOL );
      for( const auto& a : accounts ) {
        total_hive += a.balance;
        total_vest += a.vesting_shares;
        total_hbd  += a.hbd_balance;
        out << std::left << std::setw( 17 ) << std::string(a.name)
            << std::right << std::setw(18) << condenser_api::legacy_asset::from_asset(a.balance).to_string() <<" "
            << std::right << std::setw(26) << condenser_api::legacy_asset::from_asset(a.vesting_shares).to_string() <<" "
            << std::right << std::setw(16) << condenser_api::legacy_asset::from_asset(a.hbd_balance).to_string() <<"\n";
      }
      out << "-------------------------------------------------------------------------\n";
        out << std::left << std::setw( 17 ) << "TOTAL"
            << std::right << std::setw(18) << condenser_api::legacy_asset::from_asset(total_hive).to_string() <<" "
            << std::right << std::setw(26) << condenser_api::legacy_asset::from_asset(total_vest).to_string() <<" "
            << std::right << std::setw(16) << condenser_api::legacy_asset::from_asset(total_hbd).to_string() <<"\n";
      return out.str();
    };
    m["get_account_history"] = []( variant result, const fc::variants& a )
    {
      std::stringstream ss;
      ss << std::left << std::setw( 5 )  << "#" << " ";
      ss << std::left << std::setw( 10 ) << "BLOCK #" << " ";
      ss << std::left << std::setw( 15 ) << "TRX ID" << " ";
      ss << std::left << std::setw( 20 ) << "OPERATION" << " ";
      ss << std::left << std::setw( 50 ) << "DETAILS" << "\n";
      ss << "-------------------------------------------------------------------------------\n";
      const auto& results = result.get_array();
      for( const auto& item : results )
      {
        ss << std::left << std::setw(5) << item.get_array()[0].as_string() << " ";
        const auto& op = item.get_array()[1].get_object();
        ss << std::left << std::setw(10) << op["block"].as_string() << " ";
        ss << std::left << std::setw(15) << op["trx_id"].as_string() << " ";
        const auto& opop = op["op"].get_array();
        ss << std::left << std::setw(20) << opop[0].as_string() << " ";
        ss << std::left << std::setw(50) << fc::json::to_string(opop[1]) << "\n ";
      }
      return ss.str();
    };
    m["get_open_orders"] = []( variant result, const fc::variants& a ) {
        auto orders = result.as<vector<database_api::api_limit_order_object>>();

        std::stringstream ss;

        ss << setiosflags( ios::fixed ) << setiosflags( ios::left ) ;
        ss << ' ' << setw( 10 ) << "Order #";
        ss << ' ' << setw( 10 ) << "Price";
        ss << ' ' << setw( 10 ) << "Quantity";
        ss << ' ' << setw( 10 ) << "Type";
        ss << "\n=====================================================================================================\n";
        for( const auto& o : orders )
        {
          ss << ' ' << setw( 10 ) << o.orderid;
          // Based on real_price = condenser_api::legacy_asset_TO_REAL( itr->sell_price.base ) / condenser_api::legacy_asset_TO_REAL( itr->sell_price.quote ); from market_history_api.
          ss << ' ' << setw( 10 ) << double(o.sell_price.base.amount.value) / double(o.sell_price.quote.amount.value);
          ss << ' ' << setw( 10 ) << condenser_api::legacy_asset::from_asset(asset(o.for_sale, o.sell_price.base.symbol)).to_string();
          ss << ' ' << setw( 10 ) << (o.sell_price.base.symbol == HIVE_SYMBOL ? "SELL" : "BUY");
          ss << "\n";
        }
        return ss.str();
    };
    m["get_order_book"] = []( variant result, const fc::variants& a ) {
      auto orders = result.as< wallet_bridge_api::get_order_book_return >();
      std::stringstream ss;
      asset bid_sum = asset( 0, HBD_SYMBOL );
      asset ask_sum = asset( 0, HBD_SYMBOL );
      int spacing = 24;

      ss << setiosflags( ios::fixed ) << setiosflags( ios::left ) ;

      ss << ' ' << setw( ( spacing * 4 ) + 6 ) << "Bids" << "Asks\n"
        << ' '
        << setw( spacing + 3 ) << "Sum(HBD)"
        << setw( spacing + 1) << "HBD"
        << setw( spacing + 1 ) << "HIVE"
        << setw( spacing + 1 ) << "Price"
        << setw( spacing + 1 ) << "Price"
        << setw( spacing + 1 ) << "HIVE "
        << setw( spacing + 1 ) << "HBD " << "Sum(HBD)"
        << "\n====================================================================================================="
        << "|=====================================================================================================\n";

      for( size_t i = 0; i < orders.bids.size() || i < orders.asks.size(); i++ )
      {
        if( i < orders.bids.size() )
        {
          bid_sum += asset( orders.bids[i].hbd, HBD_SYMBOL );
          ss
            << ' ' << setw( spacing ) << condenser_api::legacy_asset::from_asset(bid_sum).to_string()
            << ' ' << setw( spacing ) << condenser_api::legacy_asset::from_asset(asset( orders.bids[i].hbd, HBD_SYMBOL)).to_string()
            << ' ' << setw( spacing ) << condenser_api::legacy_asset::from_asset(asset( orders.bids[i].hive, HIVE_SYMBOL)).to_string()
            << ' ' << setw( spacing ) << orders.bids[i].real_price;
        }
        else
        {
          ss << setw( (spacing * 4 ) + 5 ) << ' ';
        }

        ss << " |";

        if( i < orders.asks.size() )
        {
          ask_sum += asset(orders.asks[i].hbd, HBD_SYMBOL);
          ss << ' ' << setw( spacing ) << orders.asks[i].real_price
            << ' ' << setw( spacing ) << condenser_api::legacy_asset::from_asset(asset( orders.asks[i].hive, HIVE_SYMBOL )).to_string()
            << ' ' << setw( spacing ) << condenser_api::legacy_asset::from_asset(asset( orders.asks[i].hbd, HBD_SYMBOL )).to_string()
            << ' ' << setw( spacing ) << condenser_api::legacy_asset::from_asset(ask_sum).to_string();
        }

        ss << endl;
      }

      ss << endl
        << "Bid Total: " << condenser_api::legacy_asset::from_asset(bid_sum).to_string() << endl
        << "Ask Total: " << condenser_api::legacy_asset::from_asset(ask_sum).to_string() << endl;

      return ss.str();
    };
    m["get_withdraw_routes"] = []( variant result, const fc::variants& a )
    {
      auto routes = result.as< vector< database_api::api_withdraw_vesting_route_object > >();
      std::stringstream ss;

      ss << ' ' << std::left << std::setw( 20 ) << "From";
      ss << ' ' << std::left << std::setw( 20 ) << "To";
      ss << ' ' << std::right << std::setw( 8 ) << "Percent";
      ss << ' ' << std::right << std::setw( 9 ) << "Auto-Vest";
      ss << "\n==============================================================\n";

      for( auto& r : routes )
      {
        ss << ' ' << std::left << std::setw( 20 ) << std::string( r.from_account );
        ss << ' ' << std::left << std::setw( 20 ) << std::string( r.to_account );
        ss << ' ' << std::right << std::setw( 8 ) << std::setprecision( 2 ) << std::fixed << double( r.percent ) / 100;
        ss << ' ' << std::right << std::setw( 9 ) << ( r.auto_vest ? "true" : "false" ) << std::endl;
      }

      return ss.str();
    };

    return m;
  }

  operation get_prototype_operation( const string& operation_name )
  {
    auto it = _prototype_ops.find( operation_name );
    if( it == _prototype_ops.end() )
      FC_THROW("Unsupported operation: \"${operation_name}\"", ("operation_name", operation_name));
    return it->second;
  }

  string                                                    _wallet_filename;
  wallet_data                                               _wallet;
  chain_id_type                                             _hive_chain_id;

  map<public_key_type,string>                               _keys;
  fc::sha512                                                _checksum;
  fc::api< hive::plugins::wallet_bridge_api::wallet_bridge_api > _remote_wallet_bridge_api;
  uint32_t                                                  _tx_expiration_seconds = 30;

  flat_map<string, operation>                               _prototype_ops;

  static_variant_map _operation_which_map = create_static_variant_map< operation >();

#ifdef __unix__
  mode_t                  _old_umask;
#endif
  const string _wallet_filename_extension = ".wallet";
};

condenser_api::legacy_signed_transaction wallet_api_impl::build_claim_account_creation(const string& creator, const condenser_api::legacy_asset& fee,
  const std::function<condenser_api::legacy_signed_transaction(signed_transaction)>& tx_signer)
{
  try
  {
    FC_ASSERT(!is_locked());

    auto creator_account = get_account(creator);
    claim_account_operation op;
    op.creator = creator;
    op.fee = fee;

    signed_transaction tx;
    tx.operations.push_back(op);
    tx.validate();

    return tx_signer(tx);
  } FC_CAPTURE_AND_RETHROW((creator))
}

} } } // hive::wallet::detail



namespace hive { namespace wallet {

wallet_api::wallet_api(const wallet_data& initial_data, const chain_id_type& hive_chain_id, const fc::api< hive::plugins::wallet_bridge_api::wallet_bridge_api >& remote_api)
  : my(new detail::wallet_api_impl(*this, initial_data, hive_chain_id, remote_api))
{}

wallet_api::~wallet_api(){}

bool wallet_api::copy_wallet_file(const string& destination_filename)
{
  return my->copy_wallet_file(destination_filename);
}

optional< block_api::api_signed_block_object > wallet_api::get_block(uint32_t num)
{
  return my->_remote_wallet_bridge_api->get_block( {num}, LOCK ).block;
}

vector< account_history::api_operation_object > wallet_api::get_ops_in_block(uint32_t block_num, bool only_virtual)
{
  vector< variant > args{block_num, only_virtual};
  const auto operations = my->_remote_wallet_bridge_api->get_ops_in_block(args , LOCK ).ops;
  vector< account_history::api_operation_object > result;
  result.reserve(operations.size());
  for (const auto& op : operations)
    result.push_back(op);

  result.shrink_to_fit();
  return result;
}

vector< database_api::api_account_object > wallet_api::list_my_accounts()
{
  FC_ASSERT( !is_locked(), "Wallet must be unlocked to list accounts" );
  vector<database_api::api_account_object> result;

  vector<public_key_type> pub_keys;
  pub_keys.reserve( my->_keys.size() );

  for( const auto& item : my->_keys )
    pub_keys.push_back(item.first);

  auto refs = my->_remote_wallet_bridge_api->list_my_accounts( {variant(pub_keys)}, LOCK ).accounts;
  set<string> names;
  for( const auto& item : refs )
    for( const auto& name : item )
      names.insert( name );


  result.reserve( names.size() );
  for( const auto& name : names )
    result.emplace_back( get_account( name ) );

  return result;
}

vector< account_name_type > wallet_api::list_accounts(const string& lowerbound, uint32_t limit)
{
  vector<variant> args{lowerbound, limit};
  const auto accounts = my->_remote_wallet_bridge_api->list_accounts( args, LOCK );
  vector<account_name_type> result;
  result.reserve(accounts.size());
  for (const auto& acc : accounts)
    result.push_back(acc);
  
  result.shrink_to_fit();
  return result;
}

vector< account_name_type > wallet_api::get_active_witnesses()const {
  return my->_remote_wallet_bridge_api->get_active_witnesses({}, LOCK).witnesses;
}

brain_key_info wallet_api::suggest_brain_key()const
{
  brain_key_info result;
  // create a private key for secure entropy
  fc::sha256 sha_entropy1 = fc::ecc::private_key::generate().get_secret();
  fc::sha256 sha_entropy2 = fc::ecc::private_key::generate().get_secret();
  fc::bigint entropy1( sha_entropy1.data(), sha_entropy1.data_size() );
  fc::bigint entropy2( sha_entropy2.data(), sha_entropy2.data_size() );
  fc::bigint entropy(entropy1);
  entropy <<= 8*sha_entropy1.data_size();
  entropy += entropy2;
  string brain_key = "";

  for( int i=0; i<BRAIN_KEY_WORD_COUNT; i++ )
  {
    fc::bigint choice = entropy % hive::words::word_list_size;
    entropy /= hive::words::word_list_size;
    if( i > 0 )
      brain_key += " ";
    brain_key += hive::words::word_list[ choice.to_int64() ];
  }

  brain_key = normalize_brain_key(brain_key);
  fc::ecc::private_key priv_key = detail::derive_private_key( brain_key, 0 );
  result.brain_priv_key = brain_key;
  result.wif_priv_key = key_to_wif( priv_key );
  result.pub_key = priv_key.get_public_key();
  return result;
}

string wallet_api::serialize_transaction( const signed_transaction& tx )const
{
  return fc::to_hex(fc::raw::pack_to_vector(tx));
}

string wallet_api::get_wallet_filename() const
{
  return my->get_wallet_filename();
}


database_api::api_account_object wallet_api::get_account( const string& account_name ) const
{
  return my->get_account( account_name );
}

bool wallet_api::import_key(const string& wif_key)
{
  FC_ASSERT(!is_locked());
  // backup wallet
  fc::optional<fc::ecc::private_key> optional_private_key = wif_to_key(wif_key);
  if( !optional_private_key )
    FC_THROW("Invalid private key");
//   string shorthash = detail::pubkey_to_shorthash( optional_private_key->get_public_key() );
//   copy_wallet_file( "before-import-key-" + shorthash );

  if( my->import_key(wif_key) )
  {
    save_wallet_file();
  //     copy_wallet_file( "after-import-key-" + shorthash );
    return true;
  }
  return false;
}

string wallet_api::normalize_brain_key(string s) const
{
  return detail::normalize_brain_key( std::move( s ) );
}

variant wallet_api::info()
{
  return my->info();
}

variant_object wallet_api::about() const
{
  return my->about();
}

/*
fc::ecc::private_key wallet_api::derive_private_key(const std::string& prefix_string, int sequence_number) const
{
  return detail::derive_private_key( prefix_string, sequence_number );
}
*/

vector< account_name_type > wallet_api::list_witnesses(const string& lowerbound, uint32_t limit)
{
  vector<variant> args{lowerbound, limit};
  const auto& witnesses = my->_remote_wallet_bridge_api->list_witnesses( {args}, LOCK ).witnesses;
  vector<account_name_type> result;
  result.reserve(witnesses.size());
  for (const auto& witness : witnesses)
    result.push_back(witness.owner);

  result.shrink_to_fit();
  return result;
}

optional< database_api::api_witness_object > wallet_api::get_witness( const string& owner_account)
{
  return my->get_witness(owner_account);
}

annotated_signed_transaction wallet_api::set_voting_proxy(const string& account_to_modify, const string& voting_account, bool broadcast /* = false */)
{ return my->set_voting_proxy(account_to_modify, voting_account, broadcast); }

void wallet_api::set_wallet_filename(string wallet_filename) { my->_wallet_filename = std::move(wallet_filename); }

annotated_signed_transaction wallet_api::sign_transaction(
  const signed_transaction& tx, bool broadcast /* = false */)
{ try {
  signed_transaction appbase_tx( tx );
  signed_transaction result = my->sign_transaction( appbase_tx, broadcast);
  return signed_transaction( result );
} FC_CAPTURE_AND_RETHROW( (tx) ) }

operation wallet_api::get_prototype_operation( const string& operation_name )
{
  return my->get_prototype_operation( operation_name );
}

string wallet_api::help()const
{
  std::vector<std::string> method_names = my->method_documentation.get_method_names();
  std::stringstream ss;
  for( const std::string& method_name : method_names )
  {
    try
    {
      ss << my->method_documentation.get_brief_description(method_name);
    }
    catch (const fc::key_not_found_exception&)
    {
      ss << method_name << " (no help available)\n";
    }
  }
  return ss.str();
}

string wallet_api::gethelp(const string& method)const
{
  fc::api<wallet_api> tmp;
  std::stringstream ss;
  ss << "\n";

  std::string doxygenHelpString = my->method_documentation.get_detailed_description(method);
  if( !doxygenHelpString.empty() )
    ss << doxygenHelpString;
  else
    ss << "No help defined for method " << method << "\n";

  return ss.str();
}

bool wallet_api::load_wallet_file( string wallet_filename )
{
  return my->load_wallet_file( std::move(wallet_filename) );
}

void wallet_api::save_wallet_file( string wallet_filename )
{
  my->save_wallet_file( std::move(wallet_filename) );
}

std::map<string,std::function<string(fc::variant,const fc::variants&)> >
wallet_api::get_result_formatters() const
{
  return my->get_result_formatters();
}

bool wallet_api::is_locked()const
{
  return my->is_locked();
}
bool wallet_api::is_new()const
{
  return my->_wallet.cipher_keys.size() == 0;
}

void wallet_api::encrypt_keys()
{
  my->encrypt_keys();
}

void wallet_api::lock()
{ try {
  FC_ASSERT( !is_locked() );
  encrypt_keys();
  for( auto& key : my->_keys )
    key.second = key_to_wif(fc::ecc::private_key());
  my->_keys.clear();
  my->_checksum = fc::sha512();
  my->self.lock_changed(true);
} FC_CAPTURE_AND_RETHROW() }

void wallet_api::unlock(const string& password)
{ try {
  FC_ASSERT(password.size() > 0);
  auto pw = fc::sha512::hash(password.c_str(), password.size());
  vector<char> decrypted = fc::aes_decrypt(pw, my->_wallet.cipher_keys);
  auto pk = fc::raw::unpack_from_vector<plain_keys>(decrypted);
  FC_ASSERT(pk.checksum == pw);
  my->_keys = std::move(pk.keys);
  my->_checksum = pk.checksum;
  my->self.lock_changed(false);
} FC_CAPTURE_AND_RETHROW() }

void wallet_api::set_password( const string& password )
{
  if( !is_new() )
    FC_ASSERT( !is_locked(), "The wallet must be unlocked before the password can be set" );
  my->_checksum = fc::sha512::hash( password.c_str(), password.size() );
  lock();
}

map<public_key_type, string> wallet_api::list_keys()
{
  FC_ASSERT(!is_locked());
  return my->_keys;
}

string wallet_api::get_private_key( public_key_type pubkey )const
{
  return key_to_wif( my->get_private_key( pubkey ) );
}

pair<public_key_type,string> wallet_api::get_private_key_from_password( const string& account, const string& role, const string& password )const {
  auto seed = account + role + password;
  FC_ASSERT( seed.size() );
  auto secret = fc::sha256::hash( seed.c_str(), seed.size() );
  auto priv = fc::ecc::private_key::regenerate( secret );
  return std::make_pair( public_key_type( priv.get_public_key() ), key_to_wif( priv ) );
}

database_api::api_feed_history_object wallet_api::get_feed_history()const { return my->_remote_wallet_bridge_api->get_feed_history({}, LOCK); }

condenser_api::legacy_signed_transaction wallet_api::claim_account_creation(const string& creator,
                                                                            const condenser_api::legacy_asset& fee,
                                                                            bool broadcast )const
{
  return my->build_claim_account_creation(creator, fee,
    [this, broadcast](signed_transaction tx) -> condenser_api::legacy_signed_transaction
    {
      return my->sign_transaction(tx, broadcast);
    }
    );
}

condenser_api::legacy_signed_transaction wallet_api::claim_account_creation_nonblocking(const string& creator,
                                                                                        const condenser_api::legacy_asset& fee,
                                                                                        bool broadcast )const
{
  return my->build_claim_account_creation(creator, fee,
    [this, broadcast](signed_transaction tx) ->condenser_api::legacy_signed_transaction
    {
    return my->sign_and_broadcast_transaction(tx, broadcast, false);
    }
  );
}

/**
  * This method is used by faucets to create new accounts for other users which must
  * provide their desired keys. The resulting account may not be controllable by this
  * wallet.
  */
annotated_signed_transaction wallet_api::create_account_with_keys(
  const string& creator,
  const string& new_account_name,
  const string& json_meta,
  public_key_type owner,
  public_key_type active,
  public_key_type posting,
  public_key_type memo,
  bool broadcast )const
{
  legacy_asset no_funds;
  return create_funded_account_with_keys(creator, new_account_name, no_funds, "", json_meta, owner,
    active, posting, memo, broadcast);
}

/**
 * This method is used by faucets to create new accounts for other users which must
 * provide their desired keys. The resulting account may not be controllable by this
 * wallet.
 */
condenser_api::legacy_signed_transaction wallet_api::create_funded_account_with_keys( const string& creator,
                                                                                      const string& new_account_name,
                                                                                      const condenser_api::legacy_asset& initial_amount,
                                                                                      const string& memo,
                                                                                      const string& json_meta,
                                                                                      public_key_type owner_key,
                                                                                      public_key_type active_key,
                                                                                      public_key_type posting_key,
                                                                                      public_key_type memo_key,
                                                                                      bool broadcast )const
{ try {
  FC_ASSERT( !is_locked() );
  account_create_operation op;
  op.creator = creator;
  op.new_account_name = new_account_name;
  op.owner = authority( 1, owner, 1 );
  op.active = authority( 1, active, 1 );
  op.posting = authority( 1, posting, 1 );
  op.memo_key = memo;
  op.json_metadata = json_meta;
  op.fee = my->_remote_wallet_bridge_api->get_chain_properties({}, LOCK).account_creation_fee;

  auto creator_account = get_account(creator);
  signed_transaction tx;
  if( creator_account.pending_claimed_accounts > 0 )
  {
    create_claimed_account_operation op;
    op.creator = creator;
    op.new_account_name = new_account_name;
    op.owner = authority( 1, owner_key, 1 );
    op.active = authority( 1, active_key, 1 );
    op.posting = authority( 1, posting_key, 1 );
    op.memo_key = memo_key;
    op.json_metadata = json_meta;

    tx.operations.push_back(op);
  }
  else
  {
    account_create_operation op;
    op.creator = creator;
    op.new_account_name = new_account_name;
    op.owner = authority( 1, owner_key, 1 );
    op.active = authority( 1, active_key, 1 );
    op.posting = authority( 1, posting_key, 1 );
    op.memo_key = memo_key;
    op.json_metadata = json_meta;
    op.fee = my->_remote_api->get_chain_properties().account_creation_fee;

    tx.operations.push_back(op);
  }
  idump((tx.operations[0]));

  if( initial_amount.amount.value > 0 )
  {
    transfer_operation transfer_op;
    transfer_op.from = creator;
    transfer_op.to = new_account_name;
    transfer_op.amount = initial_amount;

    if( memo.size() > 0 && memo[0] == '#' )
    {
      auto from_account = get_account( creator );
      transfer_op.memo = get_encrypted_memo_using_keys( from_account.memo_key, memo_key, memo );
    }
    else
    {
      transfer_op.memo = memo;
    }

    tx.operations.push_back(transfer_op);
    idump((tx.operations[1]));
  }

  tx.validate();

  return my->sign_transaction( tx, broadcast );
} FC_CAPTURE_AND_RETHROW( (creator)(new_account_name)(json_meta)(owner)(active)(memo)(broadcast) ) }


/**
  * This method is used by faucets to create new accounts for other users which must
  * provide their desired keys. The resulting account may not be controllable by this
  * wallet.
  */
annotated_signed_transaction wallet_api::create_account_with_keys_delegated(
  const string& creator,
  const condenser_api::legacy_asset& hive_fee,
  const condenser_api::legacy_asset& delegated_vests,
  const string& new_account_name,
  const string& json_meta,
  public_key_type owner,
  public_key_type active,
  public_key_type posting,
  public_key_type memo,
  bool broadcast )const
{ try {
  FC_ASSERT( !is_locked() );
  account_create_with_delegation_operation op;
  op.creator = creator;
  op.new_account_name = new_account_name;
  op.owner = authority( 1, owner, 1 );
  op.active = authority( 1, active, 1 );
  op.posting = authority( 1, posting, 1 );
  op.memo_key = memo;
  op.json_metadata = json_meta;
  op.fee = hive_fee.to_asset();
  op.delegation = delegated_vests.to_asset();

  signed_transaction tx;
  tx.operations.push_back(op);
  tx.validate();

  return my->sign_transaction( tx, broadcast );
} FC_CAPTURE_AND_RETHROW( (creator)(new_account_name)(json_meta)(owner)(active)(memo)(broadcast) ) }

annotated_signed_transaction wallet_api::request_account_recovery( const string& recovery_account, const string& account_to_recover, authority new_authority, bool broadcast )
{
  FC_ASSERT( !is_locked() );
  request_account_recovery_operation op;
  op.recovery_account = recovery_account;
  op.account_to_recover = account_to_recover;
  op.new_owner_authority = std::move(new_authority);

  signed_transaction tx;
  tx.operations.push_back(op);
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::recover_account( const string& account_to_recover, authority recent_authority, authority new_authority, bool broadcast ) {
  FC_ASSERT( !is_locked() );

  recover_account_operation op;
  op.account_to_recover = account_to_recover;
  op.new_owner_authority = std::move(new_authority);
  op.recent_owner_authority = std::move(recent_authority);

  signed_transaction tx;
  tx.operations.push_back(op);
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::change_recovery_account( const string& owner, const string& new_recovery_account, bool broadcast ) {
  FC_ASSERT( !is_locked() );

  change_recovery_account_operation op;
  op.account_to_recover = owner;
  op.new_recovery_account = new_recovery_account;

  signed_transaction tx;
  tx.operations.push_back(op);
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

vector< database_api::api_owner_authority_history_object > wallet_api::get_owner_history( const string& account )const
{
  return my->_remote_wallet_bridge_api->get_owner_history( {account}, LOCK ).owner_auths;
}

annotated_signed_transaction wallet_api::update_account(
  const string& account_name,
  const string& json_meta,
  public_key_type owner,
  public_key_type active,
  public_key_type posting,
  public_key_type memo,
  bool broadcast )const
{
  try
  {
    FC_ASSERT( !is_locked() );

    account_update_operation op;
    op.account = account_name;
    op.owner  = authority( 1, owner, 1 );
    op.active = authority( 1, active, 1);
    op.posting = authority( 1, posting, 1);
    op.memo_key = memo;
    op.json_metadata = json_meta;

    signed_transaction tx;
    tx.operations.push_back(op);
    tx.validate();

    return my->sign_transaction( tx, broadcast );
  }
  FC_CAPTURE_AND_RETHROW( (account_name)(json_meta)(owner)(active)(memo)(broadcast) )
}

annotated_signed_transaction wallet_api::update_account_auth_key(
  const string& account_name,
  authority_type type,
  public_key_type key,
  weight_type weight,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );

  auto account = my->_remote_wallet_bridge_api->get_account( { account_name }, LOCK );
  FC_ASSERT( account.valid(), "Account does not exist" );
  FC_ASSERT( account_name == account->name, "Account name doesn't match?" );

  account_update_operation op;
  op.account = account_name;
  op.memo_key = account->memo_key;
  op.json_metadata = account->json_metadata;

  authority new_auth;

  switch( type )
  {
    case( owner ):
      new_auth = account->owner;
      break;
    case( active ):
      new_auth = account->active;
      break;
    case( posting ):
      new_auth = account->posting;
      break;
  }

  if( weight == 0 ) // Remove the key
  {
    new_auth.key_auths.erase( key );
  }
  else
  {
    new_auth.add_authority( key, weight );
  }

  if( new_auth.is_impossible() )
  {
    if( type == owner )
    {
      FC_ASSERT( false, "Owner authority change would render account irrecoverable." );
    }

    wlog( "Authority is now impossible." );
  }

  switch( type )
  {
    case( owner ):
      op.owner = new_auth;
      break;
    case( active ):
      op.active = new_auth;
      break;
    case( posting ):
      op.posting = new_auth;
      break;
  }

  signed_transaction tx;
  tx.operations.push_back(op);
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::update_account_auth_account(
  const string& account_name,
  authority_type type,
  const string& auth_account,
  weight_type weight,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );

  auto account = my->_remote_wallet_bridge_api->get_account( { account_name }, LOCK );
  FC_ASSERT( account.valid(), "Account does not exist" );
  FC_ASSERT( account_name == account->name, "Account name doesn't match?" );

  account_update_operation op;
  op.account = account_name;
  op.memo_key = account->memo_key;
  op.json_metadata = account->json_metadata;

  authority new_auth;

  switch( type )
  {
    case( owner ):
      new_auth = account->owner;
      break;
    case( active ):
      new_auth = account->active;
      break;
    case( posting ):
      new_auth = account->posting;
      break;
  }

  if( weight == 0 ) // Remove the key
  {
    new_auth.account_auths.erase( auth_account );
  }
  else
  {
    new_auth.add_authority( auth_account, weight );
  }

  if( new_auth.is_impossible() )
  {
    if( type == owner )
    {
      FC_ASSERT( false, "Owner authority change would render account irrecoverable." );
    }

    wlog( "Authority is now impossible." );
  }

  switch( type )
  {
    case( owner ):
      op.owner = new_auth;
      break;
    case( active ):
      op.active = new_auth;
      break;
    case( posting ):
      op.posting = new_auth;
      break;
  }

  signed_transaction tx;
  tx.operations.push_back(op);
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::update_account_auth_threshold(
  const string& account_name,
  authority_type type,
  uint32_t threshold,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );

  auto account = my->_remote_wallet_bridge_api->get_account( { account_name }, LOCK );
  FC_ASSERT( account.valid(), "Account does not exist" );
  FC_ASSERT( account_name == account->name, "Account name doesn't match?" );
  FC_ASSERT( threshold != 0, "Authority is implicitly satisfied" );

  account_update_operation op;
  op.account = account_name;
  op.memo_key = account->memo_key;
  op.json_metadata = account->json_metadata;

  authority new_auth;

  switch( type )
  {
    case( owner ):
      new_auth = account->owner;
      break;
    case( active ):
      new_auth = account->active;
      break;
    case( posting ):
      new_auth = account->posting;
      break;
  }

  new_auth.weight_threshold = threshold;

  if( new_auth.is_impossible() )
  {
    if( type == owner )
    {
      FC_ASSERT( false, "Owner authority change would render account irrecoverable." );
    }

    wlog( "Authority is now impossible." );
  }

  switch( type )
  {
    case( owner ):
      op.owner = new_auth;
      break;
    case( active ):
      op.active = new_auth;
      break;
    case( posting ):
      op.posting = new_auth;
      break;
  }

  signed_transaction tx;
  tx.operations.push_back(op);
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::update_account_meta(
  const string& account_name,
  const string& json_meta,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );

  auto account = my->_remote_wallet_bridge_api->get_account( { account_name }, LOCK );
  FC_ASSERT( account.valid(), "Account does not exist" );
  FC_ASSERT( account_name == account->name, "Account name doesn't match?" );

  account_update_operation op;
  op.account = account_name;
  op.memo_key = account->memo_key;
  op.json_metadata = json_meta;

  signed_transaction tx;
  tx.operations.push_back(op);
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::update_account_memo_key(
  const string& account_name,
  public_key_type key,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );

  auto account = my->_remote_wallet_bridge_api->get_account( { account_name }, LOCK );
  FC_ASSERT( account.valid(), "Account does not exist" );
  FC_ASSERT( account_name == account->name, "Account name doesn't match?" );

  account_update_operation op;
  op.account = account_name;
  op.memo_key = key;
  op.json_metadata = account->json_metadata;

  signed_transaction tx;
  tx.operations.push_back(op);
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::delegate_vesting_shares(
  const string& delegator,
  const string& delegatee,
  const condenser_api::legacy_asset& vesting_shares,
  optional<condenser_api::legacy_asset> transfer_amount,
  optional<string> transfer_memo,
  bool broadcast,
  bool blocking )
{
  FC_ASSERT( !is_locked() );
  vector<variant> args{delegator, delegatee};
  auto accounts = my->_remote_wallet_bridge_api->get_accounts( { args  }, LOCK );
  FC_ASSERT( accounts.size() == 2 , "One or more of the accounts specified do not exist." );
  FC_ASSERT( delegator == accounts[0].name, "Delegator account is not right?" );
  FC_ASSERT( delegatee == accounts[1].name, "Delegator account is not right?" );

  delegate_vesting_shares_operation op;
  op.delegator = delegator;
  op.delegatee = delegatee;
  op.vesting_shares = vesting_shares.to_asset();

  signed_transaction tx;
  tx.operations.push_back( op );

  if( transfer_amount && transfer_amount->amount.value > 0 )
  {
    transfer_operation transfer_op;
    transfer_op.from = delegator;
    transfer_op.to = delegatee;
    transfer_op.amount = *transfer_amount;
    transfer_op.memo = transfer_memo ? get_encrypted_memo(delegator, delegatee, *transfer_memo) : "";

    tx.operations.push_back(transfer_op);
  }

  tx.validate();

  return my->sign_and_broadcast_transaction( tx, broadcast, blocking );
}

condenser_api::legacy_signed_transaction wallet_api::delegate_vesting_shares(
  const string& delegator,
  const string& delegatee,
  const condenser_api::legacy_asset& vesting_shares,
  bool broadcast )
{
  return delegate_vesting_shares_and_transfer_and_broadcast(delegator, delegatee, vesting_shares,
    optional<condenser_api::legacy_asset>(), optional<string>(), broadcast, true);
}

condenser_api::legacy_signed_transaction wallet_api::delegate_vesting_shares_nonblocking(
  const string& delegator,
  const string& delegatee,
  const condenser_api::legacy_asset& vesting_shares,
  bool broadcast )
{
  return delegate_vesting_shares_and_transfer_and_broadcast(delegator, delegatee, vesting_shares,
    optional<condenser_api::legacy_asset>(), optional<string>(), broadcast, false);
}

condenser_api::legacy_signed_transaction wallet_api::delegate_vesting_shares_and_transfer(
  const string& delegator,
  const string& delegatee,
  const condenser_api::legacy_asset& vesting_shares,
  const condenser_api::legacy_asset& transfer_amount,
  optional<string> transfer_memo,
  bool broadcast )
{
  return delegate_vesting_shares_and_transfer_and_broadcast(delegator, delegatee, vesting_shares,
    transfer_amount, transfer_memo, broadcast, true);
}

condenser_api::legacy_signed_transaction wallet_api::delegate_vesting_shares_and_transfer_nonblocking(
  const string& delegator,
  const string& delegatee,
  const condenser_api::legacy_asset& vesting_shares,
  const condenser_api::legacy_asset& transfer_amount,
  optional<string> transfer_memo,
  bool broadcast )
{
  return delegate_vesting_shares_and_transfer_and_broadcast( delegator, delegatee, vesting_shares,
    transfer_amount, transfer_memo, broadcast, false);
}


/**
  *  This method will genrate new owner, active, and memo keys for the new account which
  *  will be controlable by this wallet.
  */
annotated_signed_transaction wallet_api::create_account(
  const string& creator,
  const string& new_account_name,
  const string& json_meta,
  bool broadcast )
{ try {
  FC_ASSERT( !is_locked() );
  auto owner = suggest_brain_key();
  auto active = suggest_brain_key();
  auto posting = suggest_brain_key();
  auto memo = suggest_brain_key();
  import_key( owner.wif_priv_key );
  import_key( active.wif_priv_key );
  import_key( posting.wif_priv_key );
  import_key( memo.wif_priv_key );
  return create_account_with_keys( creator, new_account_name, json_meta, owner.pub_key, active.pub_key, posting.pub_key, memo.pub_key, broadcast );
} FC_CAPTURE_AND_RETHROW( (creator)(new_account_name)(json_meta) ) }

/**
  *  This method will genrate new owner, active, and memo keys for the new account which
  *  will be controlable by this wallet.
  */
annotated_signed_transaction wallet_api::create_account_delegated(
  const string& creator,
  const condenser_api::legacy_asset& hive_fee,
  const condenser_api::legacy_asset& delegated_vests,
  const string& new_account_name,
  const string& json_meta,
  bool broadcast )
{ try {
  FC_ASSERT( !is_locked() );
  auto owner = suggest_brain_key();
  auto active = suggest_brain_key();
  auto posting = suggest_brain_key();
  auto memo = suggest_brain_key();
  import_key( owner.wif_priv_key );
  import_key( active.wif_priv_key );
  import_key( posting.wif_priv_key );
  import_key( memo.wif_priv_key );
  return create_account_with_keys_delegated( creator, hive_fee, delegated_vests, new_account_name, json_meta,  owner.pub_key, active.pub_key, posting.pub_key, memo.pub_key, broadcast );
} FC_CAPTURE_AND_RETHROW( (creator)(new_account_name)(json_meta) ) }


annotated_signed_transaction wallet_api::update_witness(
  const string& witness_account_name,
  const string& url,
  public_key_type block_signing_key,
  const legacy_chain_properties& props,
  bool broadcast  )
{
  FC_ASSERT( !is_locked() );

  witness_update_operation op;

  optional< database_api::api_witness_object > wit = my->_remote_wallet_bridge_api->get_witness( {witness_account_name}, LOCK );
  if( !wit.valid() )
  {
    op.url = url;
  }
  else
  {
    FC_ASSERT( wit->owner == witness_account_name );
    if( url != "" )
      op.url = url;
    else
      op.url = wit->url;
  }
  op.owner = witness_account_name;
  op.block_signing_key = block_signing_key;
  op.props = props;

  signed_transaction tx;
  tx.operations.push_back(op);
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::vote_for_witness(
  const string& voting_account,
  const string& witness_to_vote_for,
  bool approve,
  bool broadcast )
{ try {
  FC_ASSERT( !is_locked() );

  account_witness_vote_operation op;
  op.account = voting_account;
  op.witness = witness_to_vote_for;
  op.approve = approve;

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
} FC_CAPTURE_AND_RETHROW( (voting_account)(witness_to_vote_for)(approve)(broadcast) ) }

void wallet_api::check_memo(
  const string& memo,
  const database_api::api_account_object& account )const
{
  vector< public_key_type > keys;

  try
  {
    // Check if memo is a private key
    keys.push_back( fc::ecc::extended_private_key::from_base58( memo ).get_public_key() );
  }
  catch( fc::parse_error_exception& ) {}
  catch( fc::assert_exception& ) {}

  // Get possible keys if memo was an account password
  string owner_seed = account.name + "owner" + memo;
  auto owner_secret = fc::sha256::hash( owner_seed.c_str(), owner_seed.size() );
  keys.push_back( fc::ecc::private_key::regenerate( owner_secret ).get_public_key() );

  string active_seed = account.name + "active" + memo;
  auto active_secret = fc::sha256::hash( active_seed.c_str(), active_seed.size() );
  keys.push_back( fc::ecc::private_key::regenerate( active_secret ).get_public_key() );

  string posting_seed = account.name + "posting" + memo;
  auto posting_secret = fc::sha256::hash( posting_seed.c_str(), posting_seed.size() );
  keys.push_back( fc::ecc::private_key::regenerate( posting_secret ).get_public_key() );

  // Check keys against public keys in authorites
  for( auto& key_weight_pair : account.owner.key_auths )
  {
    for( auto& key : keys )
      FC_ASSERT( key_weight_pair.first != key, "Detected private owner key in memo field. Cancelling transaction." );
  }

  for( auto& key_weight_pair : account.active.key_auths )
  {
    for( auto& key : keys )
      FC_ASSERT( key_weight_pair.first != key, "Detected private active key in memo field. Cancelling transaction." );
  }

  for( auto& key_weight_pair : account.posting.key_auths )
  {
    for( auto& key : keys )
      FC_ASSERT( key_weight_pair.first != key, "Detected private posting key in memo field. Cancelling transaction." );
  }

  const auto& memo_key = account.memo_key;
  for( auto& key : keys )
    FC_ASSERT( memo_key != key, "Detected private memo key in memo field. Cancelling transaction." );

  // Check against imported keys
  for( auto& key_pair : my->_keys )
  {
    for( auto& key : keys )
      FC_ASSERT( key != key_pair.first, "Detected imported private key in memo field. Cancelling trasanction." );
  }
}

string wallet_api::get_encrypted_memo_using_keys( const public_key_type& from_key, const public_key_type& to_key, string memo ) const
{
  FC_ASSERT( memo.size() > 0 && memo[0] == '#' );
  memo_data m;

  m.from            = from_key;
  m.to              = to_key;
  m.nonce = fc::time_point::now().time_since_epoch().count();

  auto from_priv = my->get_private_key( m.from );
  auto shared_secret = from_priv.get_shared_secret( m.to );

  fc::sha512::encoder enc;
  fc::raw::pack( enc, m.nonce );
  fc::raw::pack( enc, shared_secret );
  auto encrypt_key = enc.result();

  m.encrypted = fc::aes_encrypt( encrypt_key, fc::raw::pack_to_vector(memo.substr(1)) );
  m.check = fc::sha256::hash( encrypt_key )._hash[0];
  return m;
}

string wallet_api::get_encrypted_memo( const string& from, const string& to, const string& memo )
{
  if( memo.size() > 0 && memo[0] == '#' )
  {
    auto from_account = get_account( from );
    auto to_account   = get_account( to );

    return get_encrypted_memo_using_keys(from_account.memo_key, to_account.memo_key, memo);
  } else {
    return memo;
  }
}

annotated_signed_transaction wallet_api::transfer(
  const string& from,
  const string& to,
  const condenser_api::legacy_asset& amount,
  const string& memo,
  bool broadcast,
  bool blocking )
{ try {
  FC_ASSERT( !is_locked() );

  check_memo( memo, get_account( from ) );
  transfer_operation op;
  op.from = from;
  op.to = to;
  op.amount = amount.to_asset();

  op.memo = get_encrypted_memo( from, to, memo );

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  my->make_transaction_unique(tx, from);
  return my->sign_and_broadcast_transaction( tx, broadcast, blocking );
} FC_CAPTURE_AND_RETHROW( (from)(to)(amount)(memo)(broadcast) ) }

annotated_signed_transaction wallet_api::escrow_transfer(
  const string& from,
  const string& to,
  const string& agent,
  uint32_t escrow_id,
  const condenser_api::legacy_asset& hbd_amount,
  const condenser_api::legacy_asset& hive_amount,
  const condenser_api::legacy_asset& fee,
  const time_point_sec& ratification_deadline,
  const time_point_sec& escrow_expiration,
  const string& json_meta,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  escrow_transfer_operation op;
  op.from = from;
  op.to = to;
  op.agent = agent;
  op.escrow_id = escrow_id;
  op.hbd_amount = hbd_amount.to_asset();
  op.hive_amount = hive_amount.to_asset();
  op.fee = fee.to_asset();
  op.ratification_deadline = ratification_deadline;
  op.escrow_expiration = escrow_expiration;
  op.json_meta = json_meta;

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::escrow_approve(
  const string& from,
  const string& to,
  const string& agent,
  const string& who,
  uint32_t escrow_id,
  bool approve,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  escrow_approve_operation op;
  op.from = from;
  op.to = to;
  op.agent = agent;
  op.who = who;
  op.escrow_id = escrow_id;

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::escrow_dispute(
  const string& from,
  const string& to,
  const string& agent,
  const string& who,
  uint32_t escrow_id,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  escrow_dispute_operation op;
  op.from = from;
  op.to = to;
  op.agent = agent;
  op.who = who;
  op.escrow_id = escrow_id;

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::escrow_release(
  const string& from,
  const string& to,
  const string& agent,
  const string& who,
  const string& receiver,
  uint32_t escrow_id,
  const condenser_api::legacy_asset& hbd_amount,
  const condenser_api::legacy_asset& hive_amount,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  escrow_release_operation op;
  op.from = from;
  op.to = to;
  op.agent = agent;
  op.who = who;
  op.receiver = receiver;
  op.escrow_id = escrow_id;
  op.hbd_amount = hbd_amount.to_asset();
  op.hive_amount = hive_amount.to_asset();

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();
  return my->sign_transaction( tx, broadcast );
}

/**
  *  Transfers into savings happen immediately, transfers from savings take 72 hours
  */
annotated_signed_transaction wallet_api::transfer_to_savings(
  const string& from,
  const string& to,
  const condenser_api::legacy_asset& amount,
  const string& memo,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  check_memo( memo, get_account( from ) );
  transfer_to_savings_operation op;
  op.from = from;
  op.to   = to;
  op.memo = get_encrypted_memo( from, to, memo );
  op.amount = amount.to_asset();

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

/**
  * @param request_id - an unique ID assigned by from account, the id is used to cancel the operation and can be reused after the transfer completes
  */
annotated_signed_transaction wallet_api::transfer_from_savings(
  const string& from,
  uint32_t request_id,
  const string& to,
  const condenser_api::legacy_asset& amount,
  const string& memo,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  check_memo( memo, get_account( from ) );
  transfer_from_savings_operation op;
  op.from = from;
  op.request_id = request_id;
  op.to = to;
  op.amount = amount.to_asset();
  op.memo = get_encrypted_memo( from, to, memo );

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

/**
  *  @param request_id the id used in transfer_from_savings
  *  @param from the account that initiated the transfer
  */
annotated_signed_transaction wallet_api::cancel_transfer_from_savings(
  const string& from,
  uint32_t request_id,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  cancel_transfer_from_savings_operation op;
  op.from = from;
  op.request_id = request_id;
  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::transfer_to_vesting(
  const string& from,
  const string& to,
  const condenser_api::legacy_asset& amount,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  transfer_to_vesting_operation op;
  op.from = from;
  op.to = (to == from ? "" : to);
  op.amount = amount.to_asset();

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  my->make_transaction_unique(tx, from);

  return my->sign_and_broadcast_transaction( tx, broadcast, blocking );
}

annotated_signed_transaction wallet_api::withdraw_vesting(
  const string& from,
  const condenser_api::legacy_asset& vesting_shares,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  withdraw_vesting_operation op;
  op.account = from;
  op.vesting_shares = vesting_shares.to_asset();

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::set_withdraw_vesting_route(
  const string& from,
  const string& to,
  uint16_t percent,
  bool auto_vest,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  set_withdraw_vesting_route_operation op;
  op.from_account = from;
  op.to_account = to;
  op.percent = percent;
  op.auto_vest = auto_vest;

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::convert_hbd(
  const string& from,
  const condenser_api::legacy_asset& amount,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  convert_operation op;
  op.owner = from;
  op.requestid = fc::time_point::now().sec_since_epoch();
  op.amount = amount.to_asset();

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::convert_hive_with_collateral(
  const string& from,
  const condenser_api::legacy_asset& collateral_amount,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  collateralized_convert_operation op;
  op.owner = from;
  op.requestid = fc::time_point::now().sec_since_epoch();
  op.amount = collateral_amount.to_asset();

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

condenser_api::legacy_asset wallet_api::estimate_hive_collateral(
  const condenser_api::legacy_asset& hbd_amount_to_get )
{
  //must reflect calculations from collateralized_convert_evaluator::do_apply

  auto fhistory = get_feed_history();
  FC_ASSERT( !static_cast<price>( fhistory.current_median_history ).is_null(), "Cannot estimate conversion collateral because there is no price feed." );

  auto needed_hive = multiply_with_fee( hbd_amount_to_get, fhistory.current_min_history,
    HIVE_COLLATERALIZED_CONVERSION_FEE, HIVE_SYMBOL );
  uint128_t _amount = ( uint128_t( needed_hive.amount.value ) * HIVE_CONVERSION_COLLATERAL_RATIO ) / HIVE_100_PERCENT;
  asset required_collateral = asset( _amount.to_uint64(), HIVE_SYMBOL );

  return condenser_api::legacy_asset::from_asset( required_collateral );
}

annotated_signed_transaction wallet_api::publish_feed(
  const string& witness,
  const price& exchange_rate,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  feed_publish_operation op;
  op.publisher     = witness;
  op.exchange_rate = price( exchange_rate );

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

vector< database_api::api_convert_request_object > wallet_api::get_conversion_requests( const string& owner_account )
{
  return my->_remote_wallet_bridge_api->get_conversion_requests( {owner_account}, LOCK );
}

vector< condenser_api::api_collateralized_convert_request_object > wallet_api::get_collateralized_conversion_requests( const string& owner_account )
{
  return my->_remote_api->get_collateralized_conversion_requests( owner_account );
}

string wallet_api::decrypt_memo( string encrypted_memo )
{
  if( is_locked() )
    return encrypted_memo;

  if( encrypted_memo.size() && encrypted_memo[0] == '#' )
  {
    auto m = memo_data::from_string( encrypted_memo );
    if( m )
    {
      fc::sha512 shared_secret;
      auto from_key = my->try_get_private_key( m->from );
      if( !from_key )
      {
        auto to_key   = my->try_get_private_key( m->to );
        if( !to_key )
          return encrypted_memo;
        shared_secret = to_key->get_shared_secret( m->from );
      }
      else
      {
        shared_secret = from_key->get_shared_secret( m->to );
      }
      fc::sha512::encoder enc;
      fc::raw::pack( enc, m->nonce );
      fc::raw::pack( enc, shared_secret );
      auto encryption_key = enc.result();

      uint32_t check = fc::sha256::hash( encryption_key )._hash[0];
      if( check != m->check )
        return encrypted_memo;

      try
      {
        vector<char> decrypted = fc::aes_decrypt( encryption_key, m->encrypted );
        return fc::raw::unpack_from_vector<std::string>( decrypted );
      } catch ( ... ){}
    }
  }
  return encrypted_memo;
}

annotated_signed_transaction wallet_api::decline_voting_rights(
  const string& account,
  bool decline,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  decline_voting_rights_operation op;
  op.account = account;
  op.decline = decline;

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::claim_reward_balance(
  const string& account,
  const condenser_api::legacy_asset& reward_hive,
  const condenser_api::legacy_asset& reward_hbd,
  const condenser_api::legacy_asset& reward_vests,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  claim_reward_balance_operation op;
  op.account = account;
  op.reward_hive = reward_hive.to_asset();
  op.reward_hbd = reward_hbd.to_asset();
  op.reward_vests = reward_vests.to_asset();

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

map< uint32_t, account_history::api_operation_object > wallet_api::get_account_history( const string& account, uint32_t from, uint32_t limit )
{
  vector<variant> args{account, from, limit};
  auto result = my->_remote_wallet_bridge_api->get_account_history( {args}, LOCK ).history;
  if( !is_locked() )
  {
    for( auto& item : result )
    {
      if( item.second.op.which() == operation::tag<transfer_operation>::value )
      {
        auto& top = item.second.op.get<transfer_operation>();
        top.memo = decrypt_memo( top.memo );
      }
      else if( item.second.op.which() == operation::tag<transfer_from_savings_operation>::value )
      {
        auto& top = item.second.op.get<transfer_from_savings_operation>();
        top.memo = decrypt_memo( top.memo );
      }
      else if( item.second.op.which() == operation::tag<transfer_to_savings_operation>::value )
      {
        auto& top = item.second.op.get<transfer_to_savings_operation>();
        top.memo = decrypt_memo( top.memo );
      }
    }
  }
  return map< uint32_t, account_history::api_operation_object >();
}

vector< database_api::api_withdraw_vesting_route_object > wallet_api::get_withdraw_routes( const string& account, database_api::sort_order_type sort_type )const
{
  vector<variant> args{account, sort_type};
  return my->_remote_wallet_bridge_api->get_withdraw_routes( {args} , LOCK ).routes;
}

wallet_bridge_api::get_order_book_return wallet_api::get_order_book( uint32_t limit )
{
  FC_ASSERT( limit <= 1000 );
  return my->_remote_wallet_bridge_api->get_order_book( {limit}, LOCK );
}

vector< database_api::api_limit_order_object > wallet_api::get_open_orders( const string& accountname )
{
  return my->_remote_wallet_bridge_api->get_open_orders( {accountname}, LOCK );
}

annotated_signed_transaction wallet_api::create_order(
  const string& owner,
  uint32_t order_id,
  const condenser_api::legacy_asset& amount_to_sell,
  const condenser_api::legacy_asset& min_to_receive,
  bool fill_or_kill,
  uint32_t expiration_sec,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  limit_order_create_operation op;
  op.owner = owner;
  op.orderid = order_id;
  op.amount_to_sell = amount_to_sell.to_asset();
  op.min_to_receive = min_to_receive.to_asset();
  op.fill_or_kill = fill_or_kill;
  op.expiration = expiration_sec ? (fc::time_point::now() + fc::seconds(expiration_sec)) : fc::time_point::maximum();

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::cancel_order(
  const string& owner,
  uint32_t orderid,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  limit_order_cancel_operation op;
  op.owner = owner;
  op.orderid = orderid;

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::post_comment(
  const string& author,
  const string& permlink,
  const string& parent_author,
  const string& parent_permlink,
  const string& title,
  const string& body,
  const string& json,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  comment_operation op;
  op.parent_author =  parent_author;
  op.parent_permlink = parent_permlink;
  op.author = author;
  op.permlink = permlink;
  op.title = title;
  op.body = body;
  op.json_metadata = json;

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

annotated_signed_transaction wallet_api::vote(
  const string& voter,
  const string& author,
  const string& permlink,
  int16_t weight,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );
  FC_ASSERT( abs(weight) <= 100, "Weight must be between -100 and 100 and not 0" );

  vote_operation op;
  op.voter = voter;
  op.author = author;
  op.permlink = permlink;
  op.weight = weight * HIVE_1_PERCENT;

  signed_transaction tx;
  tx.operations.push_back( op );
  tx.validate();

  return my->sign_transaction( tx, broadcast );
}

void wallet_api::set_transaction_expiration(uint32_t seconds)
{
  my->set_transaction_expiration(seconds);
}

annotated_signed_transaction wallet_api::get_transaction( transaction_id_type id )const
{
  return my->_remote_wallet_bridge_api->get_transaction( {variant(id)}, LOCK );
}

annotated_signed_transaction wallet_api::follow( const string& follower, string following, set<string> what, bool broadcast )
{
  get_account( follower );
  FC_ASSERT( !following.empty() );
  get_account( following );

  follow::follow_operation fop;
  fop.follower = follower;
  fop.following = '@' + following;
  fop.what = std::move( what );
  follow::follow_plugin_operation op = fop;

  custom_json_operation jop;
  jop.id = "follow";
  jop.json = fc::json::to_string(op);
  jop.required_posting_auths.insert(follower);

  signed_transaction trx;
  trx.operations.push_back( jop );
  trx.validate();

  return my->sign_transaction( trx, broadcast );
}

  annotated_signed_transaction  wallet_api::create_proposal(
    const account_name_type& creator,
    const account_name_type& receiver,
    time_point_sec start_date,
    time_point_sec end_date,
    const condenser_api::legacy_asset& daily_pay,
    string subject,
    string permlink,
    bool broadcast )
  {
    FC_ASSERT( !is_locked() );

    create_proposal_operation cp;
    cp.creator = creator;
    cp.receiver = receiver;
    cp.start_date = start_date;
    cp.end_date = end_date;
    cp.daily_pay = daily_pay.to_asset();
    cp.subject = std::move( subject );
    cp.permlink = std::move( permlink );

    signed_transaction trx;
    trx.operations.push_back( cp );
    trx.validate();
    return my->sign_transaction( trx, broadcast );
  }

  annotated_signed_transaction  wallet_api::update_proposal(
    int64_t proposal_id,
    const account_name_type& creator,
    const condenser_api::legacy_asset& daily_pay,
    string subject,
    string permlink,
    bool broadcast )
  {
    FC_ASSERT( !is_locked() );

condenser_api::legacy_signed_transaction  wallet_api::update_proposal(
  int64_t proposal_id,
  const account_name_type& creator,
  const condenser_api::legacy_asset& daily_pay,
  string subject,
  string permlink,
  optional<time_point_sec> end_date,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );

    up.proposal_id = proposal_id;
    up.creator = creator;
    up.daily_pay = daily_pay.to_asset();
    up.subject = std::move(subject);
    up.permlink = std::move(permlink);

  up.proposal_id = proposal_id;
  up.creator = creator;
  up.daily_pay = daily_pay;
  up.subject = std::move(subject);
  up.permlink = std::move(permlink);
  if( end_date )
  {
    update_proposal_end_date ped;
    ped.end_date = *end_date;
    up.extensions.insert(ped);
  }

  annotated_signed_transaction  wallet_api::update_proposal_votes(
    const account_name_type& voter,
    const flat_set< int64_t >& proposals,
    bool approve,
    bool broadcast )
  {
    FC_ASSERT( !is_locked() );

condenser_api::legacy_signed_transaction  wallet_api::update_proposal_votes(
  const account_name_type& voter,
  const flat_set< int64_t >& proposals,
  bool approve,
  bool broadcast )
{
  FC_ASSERT( !is_locked() );

  update_proposal_votes_operation upv;

  upv.voter = voter;
  upv.proposal_ids = proposals;
  upv.approve = approve;

  vector< database_api::api_proposal_object > wallet_api::list_proposals(
    fc::variant start,
    uint32_t limit,
    database_api::sort_order_type order_by,
    database_api::order_direction_type order_type,
    database_api::proposal_status status )
  {
    vector<variant> args{std::move(start), limit, order_by, order_type, status};
    return my->_remote_wallet_bridge_api->list_proposals( {args}, LOCK ).proposals;
  }

  vector< database_api::api_proposal_object > wallet_api::find_proposals( vector< database_api::api_id_type > proposal_ids )
  {
    return my->_remote_wallet_bridge_api->find_proposals( {variant(std::move( proposal_ids ))}, LOCK ).proposals;
  }

  vector< database_api::api_proposal_vote_object > wallet_api::list_proposal_votes(
    fc::variant start,
    uint32_t limit,
    database_api::sort_order_type order_by,
    database_api::order_direction_type order_type,
    database_api::proposal_status status )
  {
    vector<variant> args{std::move( start ), limit, order_by, order_type, status};
    return my->_remote_wallet_bridge_api->list_proposal_votes( {args}, LOCK ).proposal_votes;
  }

   annotated_signed_transaction wallet_api::remove_proposal(const account_name_type& deleter,
                                                const flat_set< int64_t >& ids, bool broadcast )
  {
    FC_ASSERT( !is_locked() );

condenser_api::legacy_signed_transaction wallet_api::remove_proposal(const account_name_type& deleter,
                                              const flat_set< int64_t >& ids, bool broadcast )
{
  FC_ASSERT( !is_locked() );

  remove_proposal_operation rp;
  rp.proposal_owner = deleter;
  rp.proposal_ids   = ids;

  signed_transaction trx;
  trx.operations.push_back( rp );
  trx.validate();
  return my->sign_transaction( trx, broadcast );
}

condenser_api::legacy_signed_transaction wallet_api::recurrent_transfer(
 const account_name_type& from,
 const account_name_type& to,
 const condenser_api::legacy_asset& amount,
 const string& memo,
 uint16_t recurrence,
 uint16_t executions,
 bool broadcast ) {
  try {
    FC_ASSERT( !is_locked() );
    check_memo( memo, get_account( from ) );
    recurrent_transfer_operation op;
    op.from = from;
    op.to = to;
    op.amount = amount.to_asset();
    op.memo = get_encrypted_memo( from, to, memo );
    op.recurrence = recurrence;
    op.executions = executions;

    signed_transaction tx;
    tx.operations.push_back( op );
    tx.validate();

    return my->sign_transaction( tx, broadcast );
  } FC_CAPTURE_AND_RETHROW( (from)(to)(amount)(memo)(recurrence)(executions)(broadcast) )
}

vector< database_api::api_recurrent_transfer_object > wallet_api::find_recurrent_transfers(const account_name_type& from)
{
  return my->_remote_api->find_recurrent_transfers( from );
}

} } // hive::wallet
