#include <core/beekeeper_wallet.hpp>

#include <fstream>

#include <fc/io/json.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/io/raw.hpp>

#include <sys/stat.h>

namespace beekeeper {

namespace detail {

class beekeeper_impl
{
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

public:
  beekeeper_wallet& self;
  beekeeper_impl( beekeeper_wallet& s )
   : self( s )
  {
  }

  virtual ~beekeeper_impl()
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

  bool is_locked()const
  {
    return _checksum == fc::sha512();
  }

  string get_wallet_filename() const { return _wallet_filename; }

  std::optional<private_key_type> try_get_private_key(const public_key_type& id)const
  {
    auto it = _keys.find(id);
    if( it != _keys.end() )
    return  it->second.first;
    return std::optional<private_key_type>();
  }

  std::optional<signature_type> try_sign_digest( const digest_type& sig_digest, const public_key_type& public_key )
  {
    auto it = _keys.find(public_key);
    if( it == _keys.end() )
    return std::optional<signature_type>();
    return it->second.first.sign_compact( sig_digest );
  }

  private_key_type get_private_key(const public_key_type& id)const
  {
    auto has_key = try_get_private_key( id );
    FC_ASSERT( has_key, "Key doesn't exist!" );
    return *has_key;
  }

  /*
    Imports the private key into the wallet
    @returns `true` if the key matches a current key `false` otherwise
  */
  std::pair<string, bool> import_key( const string& wif_key, const string& prefix )
  {
    auto priv = private_key_type::wif_to_key( wif_key );
    if( !priv.valid() )
    {
      FC_ASSERT( false, "Key can't be constructed" );
    }

    public_key_type _wif_pub_key = priv->get_public_key();

    key_detail_pair _new_item = std::make_pair( _wif_pub_key, std::make_pair( *priv, prefix ) );
    std::string _str_wif_pub_key = utility::public_key::to_string( _new_item );

    auto itr = _keys.find( _wif_pub_key );
    if( itr == _keys.end() )
    {
      _keys.emplace( _new_item );
      return { _str_wif_pub_key, true };
    }

    //An attempt of inserting again the same key shouldn't be treated as an error.
    ilog( "A key '${key}' is already in a wallet", ("key", _str_wif_pub_key) );

    return { _str_wif_pub_key, false };
  }

  std::vector<string> import_keys( const std::vector<string>& wif_keys, const string& prefix )
  {
    std::vector<string> _result;

    for( auto& wif_key : wif_keys )
    {
      auto _status = import_key( wif_key, prefix );
      if( _status.second )
        _result.emplace_back( _status.first );
    }

    return _result;
  }

  // Removes a key from the wallet
  // @returns true if the key matches a current active/owner/memo key for the named
  //     account, false otherwise (but it is removed either way)
  bool remove_key( const public_key_type& public_key )
  {
    auto itr = _keys.find( public_key );
    if( itr != _keys.end() )
    {
      _keys.erase( public_key );
      return true;
    }
    FC_ASSERT( false, "Key not in wallet" );
  }

  bool load_wallet_file(string wallet_filename = "")
  {
    // TODO:  Merge imported wallet with existing wallet,
    //    instead of replacing it
    if( wallet_filename == "" )
      wallet_filename = _wallet_filename;

    if( ! fc::exists( wallet_filename ) )
      return false;

    _wallet = fc::json::from_file( wallet_filename, fc::json::format_validation_mode::full ).as< wallet_data >();

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
      std::ofstream outfile{ wallet_filename };
      if (!outfile)
      {
        elog("Unable to open file: ${fn}", ("fn", wallet_filename));
        FC_ASSERT( false, "Unable to open file: ${fn}", ("fn", wallet_filename));
      }
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

  string        _wallet_filename;
  wallet_data   _wallet;

  keys_details  _keys;
  fc::sha512    _checksum;

#ifdef __unix__
  mode_t        _old_umask;
#endif
  const string _wallet_filename_extension = ".wallet";
};

}

beekeeper_wallet::beekeeper_wallet( const std::string& name )
  : beekeeper_wallet_base( name ), my(new detail::beekeeper_impl( *this ))
{}

beekeeper_wallet::~beekeeper_wallet() {}

string beekeeper_wallet::get_wallet_filename() const
{
  return my->get_wallet_filename();
}

string beekeeper_wallet::import_key( const string& wif_key, const string& prefix )
{
  FC_ASSERT( !is_locked(), "Unable to import key on a locked wallet");

  const auto _result = my->import_key( wif_key, prefix );

  if( _result.second )//Save a file only when a key is really imported.
    save_wallet_file();

  return _result.first;
}

std::vector<string> beekeeper_wallet::import_keys( const std::vector<string>& wif_keys, const string& prefix )
{
  FC_ASSERT( !is_locked(), "Unable to import key on a locked wallet");

  const auto _result = my->import_keys( wif_keys, prefix );

  if( !_result.empty() )//Save a file only when at least one key is really imported.
    save_wallet_file();

  return _result;
}

bool beekeeper_wallet::remove_key( const public_key_type& public_key )
{
  FC_ASSERT( !is_locked(), "Unable to remove key from a locked wallet");

  if( my->remove_key( public_key ) )
  {
    save_wallet_file();
    return true;
  }
  return false;
}

bool beekeeper_wallet::load_wallet_file( string wallet_filename )
{
  return my->load_wallet_file( wallet_filename );
}

void beekeeper_wallet::save_wallet_file( string wallet_filename )
{
  my->save_wallet_file( wallet_filename );
  on_update();
}

bool beekeeper_wallet::is_locked() const
{
  return my->is_locked();
}

bool beekeeper_wallet::is_new() const
{
  return my->_wallet.cipher_keys.size() == 0;
}

void beekeeper_wallet::encrypt_keys()
{
  my->encrypt_keys();
}

void beekeeper_wallet::lock()
{
  FC_ASSERT( !is_locked(), "Unable to lock a locked wallet" );

  encrypt_keys();

  for( auto key : my->_keys )
    key.second = std::make_pair( private_key_type(), "" );

  my->_keys.clear();
  my->_checksum = fc::sha512();
}

bool beekeeper_wallet::is_checksum_valid( const fc::sha512& old_checksum, const vector<char>& content )
{
  fc::sha512 _new_checksum;
  fc::raw::unpack_from_vector<fc::sha512>( content, _new_checksum );

  return old_checksum == _new_checksum;
}

void beekeeper_wallet::unlock(string password)
{
  FC_ASSERT(password.size() > 0);
  auto pw = fc::sha512::hash(password.c_str(), password.size());

  vector<char> _decrypted;
  try
  {
    //Notice that `fc::aes_decrypt` sometimes doesn't throw an exception when a password is invalid.
    _decrypted = fc::aes_decrypt(pw, my->_wallet.cipher_keys);

    /*
      Details why a checksum should be calculated are in:
        PROJECT_DIR/programs/beekeeper/beekeeper/documentation/wallet-example
    */
    FC_ASSERT( is_checksum_valid( pw, _decrypted ) );
  }
  catch(...)
  {
    FC_ASSERT( false, "Invalid password for wallet: ${wallet_name}", ("wallet_name", get_wallet_filename()) );
  }

  plain_keys _pk;
  fc::raw::unpack_from_vector<plain_keys>( _decrypted, _pk );

  my->_keys     = std::move( _pk.keys );
  my->_checksum = _pk.checksum;
}

void beekeeper_wallet::check_password(string password)
{
  FC_ASSERT(password.size() > 0);
  auto pw = fc::sha512::hash(password.c_str(), password.size());

  vector<char> _decrypted;
  try
  {
    //Notice that `fc::aes_decrypt` sometimes doesn't throw an exception when a password is invalid.
    _decrypted = fc::aes_decrypt(pw, my->_wallet.cipher_keys);

    /*
      Details why a checksum should be calculated are in:
        PROJECT_DIR/programs/beekeeper/beekeeper/documentation/wallet-example
    */
    FC_ASSERT( is_checksum_valid( pw, _decrypted ) );
  }
  catch(...)
  {
    FC_ASSERT( false, "Invalid password for wallet: ${wallet_name}", ("wallet_name", get_wallet_filename()) );
  }
}

void beekeeper_wallet::set_password( string password )
{
  if( !is_new() )
    FC_ASSERT( !is_locked(), "The wallet must be unlocked before the password can be set" );
  my->_checksum = fc::sha512::hash( password.c_str(), password.size() );
  lock();
}

keys_details beekeeper_wallet::list_keys()
{
  FC_ASSERT( !is_locked(), "Unable to list public keys of a locked wallet");
  return my->_keys;
}

keys_details beekeeper_wallet::list_public_keys()
{
  FC_ASSERT( !is_locked(), "Unable to list public keys of a locked wallet");
  return my->_keys;
}

private_key_type beekeeper_wallet::get_private_key( public_key_type pubkey )const
{
  return my->get_private_key( pubkey );
}

std::optional<signature_type> beekeeper_wallet::try_sign_digest( const digest_type& sig_digest, const public_key_type& public_key )
{
  return my->try_sign_digest( sig_digest, public_key );
}

pair<public_key_type,private_key_type> beekeeper_wallet::get_private_key_from_password( string account, string role, string password )const
{
  auto seed = account + role + password;
  FC_ASSERT( seed.size(), "seed should not be empty" );
  auto secret = fc::sha256::hash( seed.c_str(), seed.size() );
  auto priv = fc::ecc::private_key::regenerate( secret );
  return std::make_pair( public_key_type( priv.get_public_key() ), priv );
}

void beekeeper_wallet::set_wallet_filename(string wallet_filename)
{
  my->_wallet_filename = wallet_filename;
}

bool beekeeper_wallet::has_matching_private_key( const public_key_type& public_key )
{
  return my->try_get_private_key( public_key ).has_value();
}

} //beekeeper_wallet
