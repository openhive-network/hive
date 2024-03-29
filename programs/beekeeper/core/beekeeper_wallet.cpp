#include <core/beekeeper_wallet.hpp>

#include <fstream>

#include <fc/io/json.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/io/raw.hpp>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>

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
    return  it->second;
    return std::optional<private_key_type>();
  }

  std::optional<signature_type> try_sign_digest( const digest_type& sig_digest, const public_key_type& public_key )
  {
    auto it = _keys.find(public_key);
    if( it == _keys.end() )
    return std::optional<signature_type>();
    return it->second.sign_compact( sig_digest );
  }

  private_key_type get_private_key(const public_key_type& id)const
  {
    auto has_key = try_get_private_key( id );
    FC_ASSERT( has_key, "Key doesn't exist!" );
    return *has_key;
  }

  // imports the private key into the wallet, and associate it in some way (?) with the
  // given account name.
  // @returns true if the key matches a current active/owner/memo key for the named
  //     account, false otherwise (but it is stored either way)
  std::pair<string, bool> import_key(string wif_key)
  {
    auto priv = private_key_type::wif_to_key( wif_key );
    if( !priv.valid() )
    {
      FC_ASSERT( false, "Key can't be constructed" );
    }

    public_key_type wif_pub_key = priv->get_public_key();
    std::string _str_wif_pub_key = utility::public_key::to_string( wif_pub_key );

    auto itr = _keys.find(wif_pub_key);
    if( itr == _keys.end() )
    {
      _keys[wif_pub_key] = *priv;
      return { _str_wif_pub_key, true };
    }

    //An attempt of inserting again the same key shouldn't be treated as an error.
    ilog( "A key '${wif_pub_key}' is already in a wallet", ("wif_pub_key", _str_wif_pub_key) );

    return { _str_wif_pub_key, false };
  }

  // Removes a key from the wallet
  // @returns true if the key matches a current active/owner/memo key for the named
  //     account, false otherwise (but it is removed either way)
  bool remove_key(string public_key)
  {
    FC_ASSERT( public_key.size() > 0 );

    public_key_type pub( public_key_type::from_base58( public_key, false/*is_sha256*/ ) );
    auto itr = _keys.find(pub);
    if( itr != _keys.end() )
    {
      _keys.erase(pub);
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

  string                _wallet_filename;
  wallet_data              _wallet;

  map<public_key_type,private_key_type>  _keys;
  fc::sha512              _checksum;

#ifdef __unix__
  mode_t        _old_umask;
#endif
  const string _wallet_filename_extension = ".wallet";
};

}

beekeeper_wallet::beekeeper_wallet()
  : my(new detail::beekeeper_impl( *this ))
{}

beekeeper_wallet::~beekeeper_wallet() {}

string beekeeper_wallet::get_wallet_filename() const
{
  return my->get_wallet_filename();
}

string beekeeper_wallet::import_key(string wif_key)
{
  FC_ASSERT( !is_locked(), "Unable to import key on a locked wallet");

  const auto _result = my->import_key( wif_key );

  if( _result.second )//Save a file only when a key is really imported.
    save_wallet_file();

  return _result.first;
}

bool beekeeper_wallet::remove_key(string key)
{
  FC_ASSERT( !is_locked(), "Unable to remove key from a locked wallet");

  if( my->remove_key(key) )
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
  try
  {
    FC_ASSERT( !is_locked(), "Unable to lock a locked wallet" );

    encrypt_keys();

    for( auto key : my->_keys )
      key.second = private_key_type();

    my->_keys.clear();
    my->_checksum = fc::sha512();

  } FC_CAPTURE_AND_RETHROW()
}

void beekeeper_wallet::unlock(string password)
{
  try
  {
    FC_ASSERT(password.size() > 0);
    auto pw = fc::sha512::hash(password.c_str(), password.size());
    vector<char> decrypted = fc::aes_decrypt(pw, my->_wallet.cipher_keys);
    plain_keys pk;
    fc::raw::unpack_from_vector<plain_keys>( decrypted, pk );
    FC_ASSERT(pk.checksum == pw);
    my->_keys = std::move(pk.keys);
    my->_checksum = pk.checksum;
  }FC_CAPTURE_AND_RETHROW( ("Invalid password for wallet: ")(get_wallet_filename()) )
}

void beekeeper_wallet::check_password(string password)
{
  try
  {
    FC_ASSERT(password.size() > 0);
    auto pw = fc::sha512::hash(password.c_str(), password.size());
    vector<char> decrypted = fc::aes_decrypt(pw, my->_wallet.cipher_keys);
    plain_keys pk;
    fc::raw::unpack_from_vector<plain_keys>( decrypted, pk );
    FC_ASSERT(pk.checksum == pw);
  }FC_CAPTURE_AND_RETHROW( ("Invalid password for wallet: ")(get_wallet_filename()) )
}

void beekeeper_wallet::set_password( string password )
{
  if( !is_new() )
    FC_ASSERT( !is_locked(), "The wallet must be unlocked before the password can be set" );
  my->_checksum = fc::sha512::hash( password.c_str(), password.size() );
  lock();
}

map<public_key_type, private_key_type> beekeeper_wallet::list_keys()
{
  FC_ASSERT( !is_locked(), "Unable to list public keys of a locked wallet");
  return my->_keys;
}

flat_set<public_key_type> beekeeper_wallet::list_public_keys()
{
  FC_ASSERT( !is_locked(), "Unable to list private keys of a locked wallet");
  flat_set<public_key_type> keys;
  boost::copy( my->_keys | boost::adaptors::map_keys, std::inserter( keys, keys.end() ) );

  return keys;
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

namespace fc
{
  void from_variant( const fc::variant& var,  beekeeper::wallet_data& vo )
  {
   from_variant( var, vo.cipher_keys );
  }

  void to_variant( const beekeeper::wallet_data& var, fc::variant& vo )
  {
    to_variant( var.cipher_keys, vo );
  }
} // fc
