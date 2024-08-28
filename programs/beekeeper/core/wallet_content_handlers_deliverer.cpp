#include <core/wallet_content_handlers_deliverer.hpp>

#include <fc/filesystem.hpp>

namespace beekeeper {

bool wallet_content_handlers_deliverer::empty( const std::string& token )
{
  const auto& _idx = items.get<by_token>();
  return _idx.find( token ) == _idx.end();
}

std::optional<const wallet_content_handler_session*> wallet_content_handlers_deliverer::find( const std::string& token, const std::string& wallet_name )
{
  const auto& _idx = items.get<by_token_wallet_name>();
  auto _itr = _idx.find( boost::make_tuple( token, wallet_name ) );

  if( _itr == _idx.end() )
    return std::optional<const wallet_content_handler_session*>();
  else
    return std::optional<const wallet_content_handler_session*>( &( *_itr ) );
}

void wallet_content_handlers_deliverer::erase( const std::string& token, const std::string& wallet_name )
{
  auto& _idx = items.get<by_token_wallet_name>();
  auto _itr = _idx.find( boost::make_tuple( token, wallet_name ) );

  if( _itr != _idx.end() )
    _idx.erase( _itr );
}

void wallet_content_handlers_deliverer::emplace_or_modify( const std::string& token, const std::string& wallet_name, bool locked, const wallet_content_handler::ptr& content )
{
  auto& _idx = items.get<by_token_wallet_name>();
  auto _itr = _idx.find( boost::make_tuple( token, wallet_name ) );

  if( _itr == _idx.end() )
    _idx.emplace( wallet_content_handler_session( token, wallet_name, locked, content ) );
  else
    _idx.modify( _itr, [&]( wallet_content_handler_session& obj )
    {
      obj.set_locked( locked );
      obj.set_content( content );
    });
}

void wallet_content_handlers_deliverer::create( const std::string& token, const std::string& wallet_name, const std::string& wallet_file_name, const std::string& password, const bool is_temporary )
{
  FC_ASSERT( !fc::exists( wallet_file_name ), "Wallet with name: '${n}' already exists at ${path}", ("n", wallet_name)("path", fc::path( wallet_file_name )) );

  auto& _idx = items.get<by_wallet_name>();
  auto _itr = _idx.find( wallet_name );
  if( _itr != _idx.end() )//If exists at least one wallet, but lack of file, every old wallet should be deleted
  {
    while( _itr != _idx.end() && _itr->get_wallet_name() == wallet_name )
    {
      if( _itr->get_content()->is_wallet_temporary() )
      {
        FC_ASSERT( false, "Wallet with name: '${n}' is temporary and already exists", ("n", wallet_name) );
      }

      auto __itr = _itr;
      ++_itr;
      _idx.erase( __itr );
    }
  }

  auto _new_item = std::make_shared<wallet_content_handler>( is_temporary );

  _new_item->set_password( password );
  _new_item->set_wallet_name( wallet_file_name );
  _new_item->unlock( password );

  _new_item->save_wallet_file();

  emplace_or_modify( token, wallet_name, false, _new_item );
}

void wallet_content_handlers_deliverer::open( const std::string& token, const std::string& wallet_name, const std::string& wallet_file_name )
{
  const auto& _idx = items.get<by_wallet_name>();
  auto _itr = _idx.find( wallet_name );
  if( _itr != _idx.end() )
  {
    emplace_or_modify( token, wallet_name, true, _itr->get_content() );
    return;
  }

  auto _new_item = std::make_shared<wallet_content_handler>();
  _new_item->set_wallet_name( wallet_file_name );
  FC_ASSERT( _new_item->load_wallet_file(), "Unable to open file: ${f}", ("f", wallet_file_name) );

  emplace_or_modify( token, wallet_name, true, _new_item );
}

} //wallet_content_handler
