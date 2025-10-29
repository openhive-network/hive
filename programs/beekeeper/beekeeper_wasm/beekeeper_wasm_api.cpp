#include <beekeeper_wasm/beekeeper_wasm_api.hpp>

#include <beekeeper_wasm/beekeeper_wasm_app.hpp>
#include <core/utilities.hpp>

#include <fc/io/json.hpp>
#include <fc/optional.hpp>

#include <boost/scope_exit.hpp>

namespace beekeeper {

  class beekeeper_api::impl
  {
  public:
    std::vector<std::string> params;

    beekeeper::beekeeper_wasm_app app;

    std::string create_params_info() const;
    beekeeper_api::init_data init_impl();
  };

  std::string beekeeper_api::impl::create_params_info() const
  {
    std::string _result = "parameters: (";

    std::for_each( params.begin(), params.end(), [&_result]( const std::string& param ){ if( !param.empty() ) _result += param + " "; } );

    return _result + ") ";
  }

  void beekeeper_api::impl_deleter::operator()(beekeeper_api::impl* ptr) const
  {
    delete ptr;
  }

  beekeeper_api::beekeeper_api( const std::vector<std::string>& _params ): empty_response( "{}" )
  {
    _impl = std::unique_ptr<impl, impl_deleter>(new impl);
    _impl->params = _params;
    _impl->params.insert( _impl->params.begin(), "" );//a fake path to exe (not used, but it's necessary)
  }

  beekeeper_api::init_data beekeeper_api::impl::init_impl()
  {
    char** _params = nullptr;
    uint32_t _result = initialization_result::ok;

    BOOST_SCOPE_EXIT(&_params)
    {
      if( _params )
        delete[] _params;
    } BOOST_SCOPE_EXIT_END

    _params = new char*[ params.size() ];
    for( size_t i = 0; i < params.size(); ++i )
      _params[i] = static_cast<char*>( params[i].data() );

    _result = app.init( params.size(), _params );

    return { _result, utility::get_revision() };
  }

  public_key_type beekeeper_api::create( const std::string& source )
  {
    return utility::public_key::create( source, prefix );
  }

  template<typename T>
  std::string beekeeper_api::to_string( const T& src, bool valid )
  {
    fc::variant _v;
    fc::to_variant( src, _v );

    std::string _key_name = valid ? "result" : "error";

    return fc::json::to_string( fc::mutable_variant_object( _key_name, fc::json::to_string( _v ) ) );
  }

  std::string beekeeper_api::exception_handler( std::function<std::string()>&& method, std::function<void(bool)>&& aux_init_method )
  {
    if( !aux_init_method )
    {
      if( !initialized )
        return to_string( std::string( "Initialization failed. API call aborted." ), initialized );
    }

    std::pair<std::string, bool> _result;

    if( aux_init_method )
    {
      _result = exception::exception_handler( std::move( method ), _impl->create_params_info() );
      aux_init_method( _result.second );
    }
    else
    {
      _result = exception::exception_handler( std::move( method ) );
    }

    if( _result.second )
      return _result.first;
    else
      return to_string( _result.first, _result.second );
  }

  std::string beekeeper_api::init()
  {
    auto _method = [&, this]()
    {
      return to_string( _impl->init_impl() );
    };
    return exception_handler( _method, [this]( bool result ){ initialized = result; } );
  }

  std::string beekeeper_api::create_session_impl( const std::optional<std::string>& salt )
  {
    auto _method = [&, this]()
    {
      create_session_return _result{ _impl->app.get_wallet_manager()->create_session( salt ) };
      return to_string( _result );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::create_session()
  {
    return create_session_impl( std::optional<std::string>() );
  }

  std::string beekeeper_api::create_session( const std::string& salt )
  {
    return create_session_impl( std::optional<std::string>( salt ) );
  }

  std::string beekeeper_api::close_session( const std::string& token )
  {
    auto _method = [&, this]()
    {
      _impl->app.get_wallet_manager()->close_session( token );
      return to_string( empty_response );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::create_impl( const std::string& token, const std::string& wallet_name, const std::optional<std::string>& password, const bool is_temporary )
  {
    auto _method = [&, this]()
    {
      create_return _result{ _impl->app.get_wallet_manager()->create( token, wallet_name, password, is_temporary ) };
      return to_string( _result );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::create( const std::string& token, const std::string& wallet_name )
  {
    return create_impl( token, wallet_name, std::optional<std::string>(), false/*is_temporary*/ );
  }

  std::string beekeeper_api::create( const std::string& token, const std::string& wallet_name, bool is_temporary )
  {
    return create_impl( token, wallet_name, std::optional<std::string>(), is_temporary );
  }

  std::string beekeeper_api::create( const std::string& token, const std::string& wallet_name, bool is_temporary, const std::string& password )
  {
    return create_impl( token, wallet_name, std::optional<std::string>( password ), is_temporary );
  }

  std::string beekeeper_api::unlock( const std::string& token, const std::string& wallet_name, const std::string& password )
  {
    auto _method = [&, this]()
    {
      _impl->app.get_wallet_manager()->unlock( token, wallet_name, password );
      return to_string( empty_response );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::open( const std::string& token, const std::string& wallet_name )
  {
    auto _method = [&, this]()
    {
      _impl->app.get_wallet_manager()->open( token, wallet_name );
      return to_string( empty_response );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::close( const std::string& token, const std::string& wallet_name )
  {
    auto _method = [&, this]()
    {
      _impl->app.get_wallet_manager()->close( token, wallet_name );
      return to_string( empty_response );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::set_timeout( const std::string& token, seconds_type seconds )
  {
    auto _method = [&, this]()
    {
      _impl->app.get_wallet_manager()->set_timeout( token, seconds );
      return to_string( empty_response );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::lock_all( const std::string& token )
  {
    auto _method = [&, this]()
    {
      _impl->app.get_wallet_manager()->lock_all( token );
      return to_string( empty_response );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::lock( const std::string& token, const std::string& wallet_name )
  {
    auto _method = [&, this]()
    {
      _impl->app.get_wallet_manager()->lock( token, wallet_name );
      return to_string( empty_response );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::import_key( const std::string& token, const std::string& wallet_name, const std::string& wif_key )
  {
    auto _method = [&, this]()
    {
      import_key_return _result = { _impl->app.get_wallet_manager()->import_key( token, wallet_name, wif_key, prefix ) };
      return to_string( _result );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::import_keys( const std::string& token, const std::string& wallet_name, const std::vector<std::string>& wif_keys )
  {
    auto _method = [&, this]()
    {
      import_keys_return _result = { _impl->app.get_wallet_manager()->import_keys( token, wallet_name, wif_keys, prefix ) };
      return to_string( _result );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::remove_key( const std::string& token, const std::string& wallet_name, const std::string& public_key )
  {
    auto _method = [&, this]()
    {
      _impl->app.get_wallet_manager()->remove_key( token, wallet_name, create( public_key ) );
      return to_string( empty_response );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::list_wallets( const std::string& token )
  {
    auto _method = [&, this]()
    {
      list_wallets_return _result = { _impl->app.get_wallet_manager()->list_created_wallets( token ) };
      return to_string( _result );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::get_public_keys_impl( const std::string& token, const std::optional<std::string>& wallet_name )
  {
    auto _method = [&, this]()
    {
      get_public_keys_return _result = { utility::get_public_keys( _impl->app.get_wallet_manager()->get_public_keys( token, wallet_name ) ) };
      return to_string( _result );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::get_public_keys( const std::string& token )
  {
    return get_public_keys_impl( token, std::optional<std::string>() );
  }

  std::string beekeeper_api::get_public_keys( const std::string& token, const std::string& wallet_name )
  {
    return get_public_keys_impl( token, std::optional<std::string>( wallet_name ) );
  }

  std::string beekeeper_api::sign_digest_impl( const std::string& token, const std::string& sig_digest, const std::string& public_key, const std::optional<std::string>& wallet_name )
  {
    auto _method = [&, this]()
    {
      signature_return _result = { _impl->app.get_wallet_manager()->sign_digest( token, wallet_name, sig_digest, create( public_key ), prefix ) };
      return to_string( _result );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::sign_digest( const std::string& token, const std::string& sig_digest, const std::string& public_key )
  {
    return sign_digest_impl( token, sig_digest, public_key, std::optional<std::string>() );
  }

  std::string beekeeper_api::sign_digest( const std::string& token, const std::string& sig_digest, const std::string& public_key, const std::string& wallet_name )
  {
    return sign_digest_impl( token, sig_digest, public_key, std::optional<std::string>( wallet_name ) );
  }

  std::string beekeeper_api::get_info( const std::string& token )
  {
    auto _method = [&, this]()
    {
      info _result = _impl->app.get_wallet_manager()->get_info( token );
      return to_string( _result );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::get_version()
  {
    auto _method = [&, this]()
    {
      version _result = _impl->app.get_wallet_manager()->get_version();
      return to_string( _result );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::has_matching_private_key( const std::string& token, const std::string& wallet_name, const std::string& public_key )
  {
    auto _method = [&, this]()
    {
      has_matching_private_key_return _result{ _impl->app.get_wallet_manager()->has_matching_private_key( token, wallet_name, create( public_key ) ) };
      return to_string( _result );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::encrypt_data( const std::string& token, const std::string& from_public_key, const std::string& to_public_key, const std::string& wallet_name, const std::string& content)
  {
    auto _method = [&, this]()
    {
      std::optional<unsigned int> implicitNonce;
      encrypt_data_return _result{ _impl->app.get_wallet_manager()->encrypt_data( token, create( from_public_key ), create( to_public_key ), wallet_name, content, implicitNonce, prefix ) };
      return to_string( _result );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::encrypt_data( const std::string& token, const std::string& from_public_key, const std::string& to_public_key, const std::string& wallet_name, const std::string& content, unsigned int nonce)
  {
    auto _method = [&, this]()
    {
      encrypt_data_return _result{ _impl->app.get_wallet_manager()->encrypt_data( token, create( from_public_key ), create( to_public_key ), wallet_name, content, nonce, prefix ) };
      return to_string( _result );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::decrypt_data( const std::string& token, const std::string& from_public_key, const std::string& to_public_key, const std::string& wallet_name, const std::string& encrypted_content )
  {
    auto _method = [&, this]()
    {
      decrypt_data_return _result{ _impl->app.get_wallet_manager()->decrypt_data( token, create( from_public_key ), create( to_public_key ), wallet_name, encrypted_content ) };
      return to_string( _result );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::has_wallet( const std::string& token, const std::string& wallet_name )
  {
    auto _method = [&, this]()
    {
      has_wallet_return _result{ _impl->app.get_wallet_manager()->has_wallet( token, wallet_name ) };
      return to_string( _result );
    };
    return exception_handler( _method );
  }

  std::string beekeeper_api::is_wallet_unlocked( const std::string& token, const std::string& wallet_name )
  {
    auto _method = [&, this]()
    {
      is_wallet_unlocked_return _result{ _impl->app.get_wallet_manager()->is_wallet_unlocked( token, wallet_name ) };
      return to_string( _result );
    };
    return exception_handler( _method );
  }
};

namespace fc
{
  void to_variant( const beekeeper::beekeeper_api::init_data& var, variant& vo )
  {
    variant v = mutable_variant_object( "status", var.status )( "version", var.version );
    vo = v;
  }

  void from_variant( const variant& var, beekeeper::beekeeper_api::init_data& vo )
  {
    auto  _obj = var.get_object();
    vo.status = _obj["status"].as_uint64();
    vo.version = _obj["version"].as_string();
  }
}

FC_REFLECT( beekeeper::beekeeper_api::init_data, (status)(version) )
