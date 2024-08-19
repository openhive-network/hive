#include <core/wallet_content_handler.hpp>

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
  wallet_content_handler& self;
  beekeeper_impl( wallet_content_handler& s )
   : self( s )
  {
  }

  virtual ~beekeeper_impl()
  {}

  void encrypt_keys()
  {
    plain_keys data;
    data.keys = _keys;
    data.checksum = _checksum;
    auto plain_txt = fc::raw::pack_to_vector(data);
    _wallet.cipher_keys = fc::aes_encrypt( data.checksum, plain_txt );
  }

  bool is_locked()const
  {
    return _checksum == fc::sha512();
  }

  const std::string& get_wallet_name() const { return _wallet_name; }

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
  std::pair<std::string, bool> import_key( const std::string& wif_key, const std::string& prefix )
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

  std::vector<std::string> import_keys( const std::vector<std::string>& wif_keys, const std::string& prefix )
  {
    std::vector<std::string> _result;

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

  bool load_wallet_file(std::string wallet_filename = "")
  {
    // TODO:  Merge imported wallet with existing wallet,
    //    instead of replacing it
    if( wallet_filename == "" )
      wallet_filename = _wallet_name;

    if( ! fc::exists( wallet_filename ) )
      return false;

    _wallet = fc::json::from_file( wallet_filename, fc::json::format_validation_mode::full ).as< wallet_data >();

    return true;
  }

  void save_wallet_file(std::string wallet_filename = "")
  {
    //
    // Serialize in memory, then save to disk
    //
    // This approach lessens the risk of a partially written wallet
    // if exceptions are thrown in serialization
    //

    encrypt_keys();

    if( wallet_filename == "" )
      wallet_filename = _wallet_name;

    wlog( "saving wallet to file ${fn}", ("fn", wallet_filename) );

    std::string data = fc::json::to_pretty_string( _wallet );
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

  string        _wallet_name;
  wallet_data   _wallet;

  keys_details  _keys;
  fc::sha512    _checksum;

#ifdef __unix__
  mode_t        _old_umask;
#endif
  const std::string _wallet_filename_extension = ".wallet";
};

}

wallet_content_handler::wallet_content_handler( bool is_temporary )
  : is_temporary( is_temporary ), my(new detail::beekeeper_impl( *this ))
{}

wallet_content_handler::~wallet_content_handler() {}

const std::string& wallet_content_handler::get_wallet_name() const
{
  return my->get_wallet_name();
}

std::string wallet_content_handler::import_key( const std::string& wif_key, const std::string& prefix )
{
  const auto _result = my->import_key( wif_key, prefix );

  if( _result.second )//Save a file only when a key is really imported.
    save_wallet_file();

  return _result.first;
}

std::vector<std::string> wallet_content_handler::import_keys( const std::vector<std::string>& wif_keys, const std::string& prefix )
{
  const auto _result = my->import_keys( wif_keys, prefix );

  if( !_result.empty() )//Save a file only when at least one key is really imported.
    save_wallet_file();

  return _result;
}

bool wallet_content_handler::remove_key( const public_key_type& public_key )
{
  if( my->remove_key( public_key ) )
  {
    save_wallet_file();
    return true;
  }
  return false;
}

bool wallet_content_handler::load_wallet_file( std::string wallet_filename )
{
  if( is_wallet_temporary() )
    return true;
  else
    return my->load_wallet_file( wallet_filename );
}

void wallet_content_handler::save_wallet_file( std::string wallet_filename )
{
  if( !is_wallet_temporary() )
    my->save_wallet_file( wallet_filename );
}

bool wallet_content_handler::is_locked() const
{
  return my->is_locked();
}

void wallet_content_handler::encrypt_keys()
{
  my->encrypt_keys();
}

void wallet_content_handler::lock()
{
  encrypt_keys();

  for( auto key : my->_keys )
    key.second = std::make_pair( private_key_type(), "" );

  my->_keys.clear();
  my->_checksum = fc::sha512();
}

bool wallet_content_handler::is_checksum_valid( const fc::sha512& old_checksum, const std::vector<char>& content )
{
  fc::sha512 _new_checksum;
  fc::raw::unpack_from_vector<fc::sha512>( content, _new_checksum );

  return old_checksum == _new_checksum;
}

void wallet_content_handler::unlock(std::string password)
{
  FC_ASSERT(password.size() > 0);
  auto pw = fc::sha512::hash(password.c_str(), password.size());

  std::vector<char> _decrypted;
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
    FC_ASSERT( false, "Invalid password for wallet: ${wallet_name}", ("wallet_name", get_wallet_name()) );
  }

  plain_keys _pk;
  fc::raw::unpack_from_vector<plain_keys>( _decrypted, _pk );

  my->_keys     = std::move( _pk.keys );
  my->_checksum = _pk.checksum;
}

void wallet_content_handler::check_password( const std::string& password )
{
  FC_ASSERT(password.size() > 0);
  auto pw = fc::sha512::hash(password.c_str(), password.size());

  std::vector<char> _decrypted;
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
    FC_ASSERT( false, "Invalid password for wallet: ${wallet_name}", ("wallet_name", get_wallet_name()) );
  }
}

void wallet_content_handler::set_password( const std::string& password )
{
  my->_checksum = fc::sha512::hash( password.c_str(), password.size() );
  lock();
}

keys_details wallet_content_handler::get_keys_details()
{
  return my->_keys;
}

private_key_type wallet_content_handler::get_private_key( public_key_type pubkey )const
{
  return my->get_private_key( pubkey );
}

std::optional<signature_type> wallet_content_handler::try_sign_digest( const digest_type& sig_digest, const public_key_type& public_key )
{
  return my->try_sign_digest( sig_digest, public_key );
}

std::pair<public_key_type,private_key_type> wallet_content_handler::get_private_key_from_password( std::string account, std::string role, std::string password )const
{
  auto seed = account + role + password;
  FC_ASSERT( seed.size(), "seed should not be empty" );
  auto secret = fc::sha256::hash( seed.c_str(), seed.size() );
  auto priv = fc::ecc::private_key::regenerate( secret );
  return std::make_pair( public_key_type( priv.get_public_key() ), priv );
}

void wallet_content_handler::set_wallet_name( const std::string& wallet_name )
{
  my->_wallet_name = wallet_name;
}

bool wallet_content_handler::has_matching_private_key( const public_key_type& public_key )
{
  return my->try_get_private_key( public_key ).has_value();
}

wallet_content_handler_lock wallet_content_handlers_deliverer::create( const std::string& wallet_name, const std::string& wallet_file_name, const std::string& password )
{
  bool _exists = fc::exists( wallet_file_name );
  FC_ASSERT( !_exists, "Wallet with name: '${n}' already exists at ${path}", ("n", wallet_name)("path", fc::path( wallet_file_name )) );

  auto _found = items.find( wallet_name );
  if( _found != items.end() )
  {
    if( !_exists )
      items.erase( _found );
    else
    {
      if( _found->second->is_locked() )
        _found->second->unlock( password );
      else
        _found->second->check_password( password );

      return wallet_content_handler_lock{ false, _found->second };
    }
  }

  auto _new_item = std::make_shared<wallet_content_handler>();

  _new_item->set_password( password );
  _new_item->set_wallet_filename( wallet_file_name );
  _new_item->unlock( password );
  _new_item->lock();
  _new_item->unlock( password );

  _new_item->save_wallet_file();

  items.insert( std::make_pair( wallet_name, _new_item ) );

  return wallet_content_handler_lock{ false, _new_item };
}

wallet_content_handler_lock wallet_content_handlers_deliverer::open( const std::string& wallet_name, const std::string& wallet_file_name )
{
  auto _found = items.find( wallet_name );
  if( _found != items.end() )
  {
    return wallet_content_handler_lock{ true, _found->second };
  }

  auto _new_item = std::make_shared<wallet_content_handler>();
  _new_item->set_wallet_filename( wallet_file_name );
  FC_ASSERT( _new_item->load_wallet_file(), "Unable to open file: ${f}", ("f", wallet_file_name) );

  items.insert( std::make_pair( wallet_name, _new_item ) );

  return wallet_content_handler_lock{ true, _new_item };
}

void wallet_content_handlers_deliverer::unlock( const std::string& wallet_name, const std::string& password, wallet_content_handler_lock& wallet )
{
  auto _found = items.find( wallet_name );
  if( _found != items.end() )
  {
    if( _found->second->is_locked() )
      _found->second->unlock( password );
    else
      _found->second->check_password( password );

    wallet.set_locked( false );
  }
}

void wallet_content_handlers_deliverer::lock( wallet_content_handler_lock& wallet )
{
  wallet.set_locked( true );
}

} //wallet_content_handler
