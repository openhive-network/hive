#include <core/wallet_manager_impl.hpp>
#include <core/wallet_content_handler.hpp>

#include <fc/filesystem.hpp>
#include <fc/crypto/crypto_data.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

namespace beekeeper {

namespace bfs = boost::filesystem;

wallet_manager_impl::wallet_manager_impl( const std::string& token, wallet_content_handlers_deliverer& content_deliverer, const boost::filesystem::path& _wallet_directory )
                                        : token( token ), content_deliverer( content_deliverer ), wallet_directory( _wallet_directory )
{
}

std::string wallet_manager_impl::gen_password()
{
  constexpr auto password_prefix = "PW";

  auto key = private_key_type::generate();
  return password_prefix + key.key_to_wif();
}

void wallet_manager_impl::valid_filename( const std::string& name )
{
  FC_ASSERT( !name.empty(), "Name of wallet is incorrect. Is empty.");

  FC_ASSERT( std::find_if(name.begin(), name.end(), !boost::algorithm::is_alnum() && !boost::algorithm::is_any_of("._-@")) == name.end(),
        "Name of wallet is incorrect. Name: ${name}. Only alphanumeric and '._-@' chars are allowed", (name) );

  FC_ASSERT( bfs::path(name).filename().string() == name,
          "Name of wallet is incorrect. Name: ${name}. File creation with given name is impossible.", (name) );
}

std::string wallet_manager_impl::create( const std::string& wallet_name, const std::optional<std::string>& password, const bool is_temporary )
{
  FC_ASSERT( !password || password->size() < max_password_length,
            "Got ${password_size} bytes, but a password limit has ${max_password_length} bytes", ("password_size", password->size())(max_password_length) );

  valid_filename( wallet_name );

  auto _wallet_file_name = create_wallet_filename( wallet_name );
  std::string _password = password ? ( *password ) : gen_password();

  content_deliverer.create( token, wallet_name, _wallet_file_name.string(), _password, is_temporary );

  return _password;
}

void wallet_manager_impl::open( const std::string& wallet_name )
{
  valid_filename( wallet_name );

  auto _wallet_file_name = create_wallet_filename( wallet_name );

  content_deliverer.open( token, wallet_name, _wallet_file_name.string() );
}

void wallet_manager_impl::close( const std::string& wallet_name )
{
  FC_ASSERT( content_deliverer.find( token, wallet_name ), "Wallet not found: ${w}", ("w", wallet_name));

  content_deliverer.erase( token, wallet_name );
}

fc::optional<private_key_type> wallet_manager_impl::find_private_key_in_given_wallet( const public_key_type& public_key, const std::string& wallet_name )
{
  keys_details _keys = list_keys_impl( wallet_name, std::string(), false/*password_is_required*/ );
  for( auto& key : _keys )
  {
    if( key.first == public_key )
      return key.second.first;
  }

  return fc::optional<private_key_type>();
}

flat_set<wallet_details> wallet_manager_impl::list_wallets_impl( const std::vector< std::string >& wallet_files )
{
  flat_set<wallet_details> _result;

  const auto& _idx = content_deliverer.items.get<by_token_wallet_name>();
  auto _itr = _idx.find( token );

  while( _itr != _idx.end() && _itr->get_token() == token )
  {
    _result.insert( wallet_details{ _itr->get_wallet_name(), !_itr->is_locked() } );
    ++_itr;
  }

  for(const auto& wallet_file_name : wallet_files )
  {
    wallet_details _wallet_detail{ wallet_file_name, false/*unlocked*/ };
    if( _result.find( _wallet_detail ) == _result.end() )
      _result.emplace( std::move( _wallet_detail ) );
  }
  return _result;
}

bool wallet_manager_impl::scan_directory( std::function<bool( const std::string& )>&& processor, const boost::filesystem::path& directory, const std::string& extension ) const
{
  boost::filesystem::directory_iterator _end_itr;
  for( boost::filesystem::directory_iterator itr( directory ); itr != _end_itr; ++itr )
  {
    if( boost::filesystem::is_regular_file( itr->path() ) )
    {
      if( itr->path().extension() == extension )
      {
        std::vector<std::string> _path_parts;
        boost::split( _path_parts, itr->path().c_str(), boost::is_any_of("/") );
        if( !_path_parts.empty() )
        {
          auto _end = *_path_parts.rbegin();
          auto _found = _end.rfind( extension );
          if( _found != std::string::npos )
          {
            if( processor( _end.substr( 0, _found ) ) )
              return true;
          }
        }
      }
    }
  }
  return false;
}

std::vector< std::string > wallet_manager_impl::list_created_wallets_impl( const boost::filesystem::path& directory, const std::string& extension ) const
{
  std::vector< std::string > _result;

  scan_directory( [&_result]( const std::string& current_wallet_name ){ _result.emplace_back( current_wallet_name ); return false; }, directory, extension );

  return _result;
}

flat_set<wallet_details> wallet_manager_impl::list_wallets()
{
  return list_wallets_impl( std::vector< std::string >() );
}

flat_set<wallet_details> wallet_manager_impl::list_created_wallets()
{
  return list_wallets_impl( list_created_wallets_impl( get_wallet_directory(), get_extension() ) );
}

keys_details wallet_manager_impl::list_keys_impl( const std::string& name, const std::string& password, bool password_is_required )
{
  auto _wallet = content_deliverer.find( token, name );
  FC_ASSERT( _wallet, "Wallet not found: ${w}", ("w", name));

  auto __wallet = *_wallet;
  FC_ASSERT( __wallet );

  FC_ASSERT( !__wallet->is_locked(), "Wallet is locked: ${w}", ("w", name));
  if( password_is_required )
    __wallet->get_content()->check_password( password ); //throws if bad password
  return __wallet->get_content()->get_keys_details();
}

keys_details wallet_manager_impl::list_keys( const std::string& name, const std::string& password )
{
  return list_keys_impl( name, password, true/*password_is_required*/ );
}

keys_details wallet_manager_impl::get_public_keys( const std::optional<std::string>& wallet_name )
{
  FC_ASSERT( !content_deliverer.empty( token ), "You don't have any wallet");

  keys_details _result;
  bool is_all_wallet_locked = true;

  auto _process_wallet = [&]( const wallet_content_handler_session& wallet )
  {
    if( !wallet.is_locked() )
    {
      _result.merge( wallet.get_content()->get_keys_details() );
    }
    is_all_wallet_locked &= wallet.is_locked();
  };

  if( wallet_name )
  {
    auto _found = content_deliverer.find( token, *wallet_name );
    if( _found )
    {
      auto __found = *_found;
      FC_ASSERT( __found );

      _process_wallet( *__found );
    }
  }
  else
  {
    const auto& _idx = content_deliverer.items.get<by_token>();
    auto _itr = _idx.find( token );

    while( _itr != _idx.end() && _itr->get_token() == token )
    {
      _process_wallet( *_itr );
      ++_itr;
    }
  }

  if( wallet_name )
    FC_ASSERT( !is_all_wallet_locked, "Wallet ${wallet_name} is locked", ("wallet_name", *wallet_name));
  else
    FC_ASSERT( !is_all_wallet_locked, "You don't have any unlocked wallet");

  return _result;
}

void wallet_manager_impl::lock_all()
{
  // no call to check_timeout since we are locking all anyway
  auto& _idx = content_deliverer.items.get<by_token>();
  auto _itr = _idx.find( token );

  while( _itr != _idx.end() && _itr->get_token() == token )
  {
    if( !_itr->get_content()->is_locked() )
    {
      _idx.modify( _itr, []( wallet_content_handler_session& obj )
      {
        obj.set_locked( true );
      });
    }
    ++_itr;
  }
}

void wallet_manager_impl::lock( const std::string& wallet_name )
{
  auto& _idx = content_deliverer.items.get<by_token_wallet_name>();
  auto _itr = _idx.find( boost::make_tuple( token, wallet_name ) );

  FC_ASSERT( _itr != _idx.end(), "Wallet not found: ${w}", ("w", wallet_name));

  FC_ASSERT( !_itr->is_locked(), "Unable to lock a locked wallet ${w}", ("w", wallet_name));

  _idx.modify( _itr, []( wallet_content_handler_session& obj )
  {
    obj.set_locked( true );
  });
}

void wallet_manager_impl::unlock( const std::string& wallet_name, const std::string& password )
{
  if( !content_deliverer.find( token, wallet_name ) )
  {
    open( wallet_name );
  }

  auto& _idx = content_deliverer.items.get<by_token_wallet_name>();
  auto _itr = _idx.find( boost::make_tuple( token, wallet_name ) );

  FC_ASSERT( _itr->is_locked(), "Wallet is already unlocked: ${w}", ("w", wallet_name));

  _idx.modify( _itr, [&password]( wallet_content_handler_session& obj )
  {
    if( obj.is_locked() )
      obj.get_content()->unlock( password );
    else
      obj.get_content()->check_password( password );

    obj.set_locked( false );
  });
}

std::string wallet_manager_impl::import_key( const std::string& name, const std::string& wif_key, const std::string& prefix )
{
  auto _wallet = content_deliverer.find( token, name );
  FC_ASSERT( _wallet, "Wallet not found: ${w}", ("w", name));

  auto __wallet = *_wallet;
  FC_ASSERT( __wallet );

  FC_ASSERT( !__wallet->is_locked(), "Wallet is locked: ${w}", ("w", name));

  return __wallet->get_content()->import_key( wif_key, prefix );
}

std::vector<std::string> wallet_manager_impl::import_keys( const std::string& name, const std::vector<std::string>& wif_keys, const std::string& prefix )
{
  auto _wallet = content_deliverer.find( token, name );
  FC_ASSERT( _wallet, "Wallet not found: ${w}", ("w", name));

  auto __wallet = *_wallet;
  FC_ASSERT( __wallet );

  FC_ASSERT( !__wallet->is_locked(), "Wallet is locked: ${w}", ("w", name));

  return __wallet->get_content()->import_keys( wif_keys, prefix );
}

void wallet_manager_impl::remove_key( const std::string& name, const public_key_type& public_key )
{
  auto _wallet = content_deliverer.find( token, name );
  FC_ASSERT( _wallet, "Wallet not found: ${w}", ("w", name));

  auto __wallet = *_wallet;
  FC_ASSERT( __wallet );

  FC_ASSERT( !__wallet->is_locked(), "Wallet is locked: ${w}", ("w", name));

  __wallet->get_content()->remove_key( public_key );
}

signature_type wallet_manager_impl::sign( std::function<std::optional<signature_type>(const wallet_content_handler_session&)>&& sign_method, const std::optional<std::string>& wallet_name, const public_key_type& public_key, const std::string& prefix )
{
  try
  {
    auto _process_wallet = [&]( const wallet_content_handler_session& wallet )
    {
      if( !wallet.is_locked() )
        return sign_method( wallet );
      return std::optional<signature_type>();
    };

    if( wallet_name )
    {
      auto _wallet = content_deliverer.find( token, *wallet_name );
      if( _wallet )
      {
        auto __wallet = *_wallet;
        FC_ASSERT( __wallet );

        auto _sig = _process_wallet( *__wallet );
        if( _sig )
          return *_sig;
      }
    }
    else
    {
      const auto& _idx = content_deliverer.items.get<by_token>();
      auto _itr = _idx.find( token );

      while( _itr != _idx.end() && _itr->get_token() == token )
      {
        auto _sig = _process_wallet( *_itr );
        if( _sig )
          return *_sig;
        ++_itr;
      }
    }
  } FC_LOG_AND_RETHROW();

  if( wallet_name )
    FC_ASSERT( false, "Public key ${public_key} not found in ${wallet} wallet", ("wallet", *wallet_name)("public_key", utility::public_key::to_string( public_key, prefix )));
  else
    FC_ASSERT( false, "Public key ${public_key} not found in unlocked wallets", ("public_key", utility::public_key::to_string( public_key, prefix )));
}

signature_type wallet_manager_impl::sign_digest( const std::optional<std::string>& wallet_name, const digest_type& sig_digest, const public_key_type& public_key, const std::string& prefix )
{
  return sign( [&]( const wallet_content_handler_session& wallet ){ return wallet.get_content()->try_sign_digest( sig_digest, public_key ); }, wallet_name, public_key, prefix );
}

bool wallet_manager_impl::has_matching_private_key( const std::string& wallet_name, const public_key_type& public_key )
{
  auto _wallet = content_deliverer.find( token, wallet_name );
  FC_ASSERT( _wallet, "Wallet not found: ${w}", ("w", wallet_name));

  auto __wallet = *_wallet;
  FC_ASSERT( __wallet );

  FC_ASSERT( !__wallet->is_locked(), "Wallet is locked: ${w}", ("w", wallet_name));

  return __wallet->get_content()->has_matching_private_key( public_key );
}

std::string wallet_manager_impl::encrypt_data( const public_key_type& from_public_key, const public_key_type& to_public_key, const std::string& wallet_name, const std::string& content, const std::optional<unsigned int>& nonce, const std::string& prefix )
{
  fc::crypto_data _cd;

  auto _private_key = find_private_key_in_given_wallet( from_public_key, wallet_name );
  if( !_private_key )
    FC_ASSERT( false, "Public key ${public_key} not found in ${wallet} wallet", ("wallet", wallet_name)("public_key", utility::public_key::to_string( from_public_key, prefix )));

  return _cd.encrypt( _private_key.value(), to_public_key, content, nonce );
}

std::string wallet_manager_impl::decrypt_data( const public_key_type& from_public_key, const public_key_type& to_public_key, const std::string& wallet_name, const std::string& encrypted_content )
{
  fc::crypto_data _cd;

  return _cd.decrypt
  (
    [&wallet_name, this]( const public_key_type& key )
    {
      return find_private_key_in_given_wallet( key, wallet_name );
    },
    from_public_key, to_public_key,
    encrypted_content
  );
}

bool wallet_manager_impl::has_wallet( const std::string& wallet_name )
{
  auto _wallet = content_deliverer.find( token, wallet_name );
  if( _wallet )
    return true;
  else
    return scan_directory( [&wallet_name]( const std::string& current_wallet_name ){ return wallet_name == current_wallet_name; }, get_wallet_directory(), get_extension() );
}

is_wallet_unlocked_return wallet_manager_impl::is_wallet_unlocked( const std::string& wallet_name )
{
  const auto& _idx = content_deliverer.items.get<by_token_wallet_name>();

  auto _itr = _idx.find( boost::make_tuple( token, wallet_name ) );

  if( _itr == _idx.end() )
    return is_wallet_unlocked_return();
  else
    return { !_itr->is_locked() };
}

} //beekeeper
