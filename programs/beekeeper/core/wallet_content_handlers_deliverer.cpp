#include <core/wallet_content_handlers_deliverer.hpp>

#include <fc/filesystem.hpp>

namespace beekeeper {

bool wallet_content_handlers_deliverer::empty( const std::string& token )
{
  auto _session_found = session_items.find( token );
  if( _session_found == session_items.end() )
    return true;

  return _session_found->second.empty();
}

wallet_content_handlers_deliverer::wallet_data wallet_content_handlers_deliverer::get_wallets( const std::string& token )
{
  auto _session_found = session_items.find( token );
  if( _session_found == session_items.end() )
    return wallet_data();

  return _session_found->second;
}

std::optional<wallet_content_handler_session::ptr> wallet_content_handlers_deliverer::find( const std::string& token, const std::string& wallet_name )
{
  auto _session_found = session_items.find( token );
  if( _session_found == session_items.end() )
    return std::optional<wallet_content_handler_session::ptr>();

  auto _session_wallet_found = _session_found->second.find( wallet_name );
  if( _session_wallet_found == _session_found->second.end() )
    return std::optional<wallet_content_handler_session::ptr>();

  return { _session_wallet_found->second };
}

void wallet_content_handlers_deliverer::erase( const std::string& token, const std::string& wallet_name )
{
  auto _session_found = session_items.find( token );
  if( _session_found == session_items.end() )
    return;

  auto _session_wallet_found = _session_found->second.find( wallet_name );
  if( _session_wallet_found == _session_found->second.end() )
    return;

  _session_found->second.erase( _session_wallet_found );
}

void wallet_content_handlers_deliverer::add( const std::string& token, const std::string& wallet_name, bool locked, wallet_content_handler::ptr& content )
{
  /*
    If we have name in our map then remove it since we want the insert to replace.
    This can happen if the wallet file is added or removed while a wallet is running.
  */
  erase( token, wallet_name );

  auto _session_found = session_items.find( token );
  if( _session_found != session_items.end() )
    _session_found->second.insert( std::make_pair( wallet_name, std::make_shared<wallet_content_handler_session>( token, wallet_name, locked, content ) ) );
  else
    session_items[ token ].insert( std::make_pair( wallet_name, std::make_shared<wallet_content_handler_session>( token, wallet_name, locked, content ) ) );
}

void wallet_content_handlers_deliverer::create( const std::string& token, const std::string& wallet_name, const std::string& wallet_file_name, const std::string& password )
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

      add( token, wallet_name, false, _found->second );
      return;
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

  add( token, wallet_name, false, _new_item );
}

void wallet_content_handlers_deliverer::open( const std::string& token, const std::string& wallet_name, const std::string& wallet_file_name )
{
  auto _found = items.find( wallet_name );
  if( _found != items.end() )
  {
    add( token, wallet_name, true, _found->second );
    return;
  }

  auto _new_item = std::make_shared<wallet_content_handler>();
  _new_item->set_wallet_filename( wallet_file_name );
  FC_ASSERT( _new_item->load_wallet_file(), "Unable to open file: ${f}", ("f", wallet_file_name) );

  items.insert( std::make_pair( wallet_name, _new_item ) );

  add( token, wallet_name, true, _new_item );
}

void wallet_content_handlers_deliverer::unlock( const std::string& wallet_name, const std::string& password, wallet_content_handler_session::ptr& wallet )
{
  auto _found = items.find( wallet_name );
  if( _found != items.end() )
  {
    if( _found->second->is_locked() )
      _found->second->unlock( password );
    else
      _found->second->check_password( password );

    wallet->set_locked( false );
  }
}

void wallet_content_handlers_deliverer::lock( wallet_content_handler_session::ptr& wallet )
{
  wallet->set_locked( true );
}

} //wallet_content_handler
