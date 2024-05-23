
#include <iostream>
#include <string>
#include <chrono>

#include <hive/protocol/types.hpp>
#include <hive/protocol/authority.hpp>
#include <hive/protocol/transaction.hpp>
#include <hive/protocol/transaction_util.hpp>

#include <fc/io/json.hpp>

#include <boost/program_options.hpp>

using hive::protocol::signed_transaction;
using hive::protocol::authority;
using hive::protocol::chain_id_type;
using hive::protocol::serialization_type;
using hive::protocol::required_authorities_type;
using hive::protocol::public_key_type;
using hive::protocol::account_name_type;

int main(int argc, char** argv, char** envp)
{
  boost::program_options::options_description options("Allowed options");

  options.add_options()("trx", boost::program_options::value<std::string>(), "Transaction");
  options.add_options()("auths", boost::program_options::value<std::string>(), "Authorities");
  options.add_options()("chain-id", boost::program_options::value<std::string>(), "Chain id");

  options.add_options()("help,h", "Print usage instructions");

  boost::program_options::variables_map _options_map;
  boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options( options).run(), _options_map );

  if( _options_map.count("help") )
  {
    std::cout << options << "\n";
    return 0;
  }

  if( !_options_map.count("trx") )
  {
    std::cout << "Transaction is required" << "\n";
    return 0;
  }

  if( !_options_map.count("auths") )
  {
    std::cout << "Authorities are required" << "\n";
    return 0;
  }

  if( !_options_map.count("chain-id") )
  {
    std::cout << "Chain id is required" << "\n";
    return 0;
  }

  auto __trx      = _options_map["trx"].as<std::string>();
  auto __auths    = _options_map["auths"].as<std::string>();
  auto __chain_id = _options_map["chain-id"].as<std::string>();

  const signed_transaction& _trx = fc::json::from_string( __trx, fc::json::format_validation_mode::full ).as< signed_transaction >();
  const authority& _auth = fc::json::from_string( __auths, fc::json::format_validation_mode::full ).as< authority >();
  const chain_id_type _chain_id( fc::sha256::hash( __chain_id.c_str() ) );

  auto _start = std::chrono::high_resolution_clock::now();

  auto _public_keys = _trx.get_signature_keys( _chain_id, serialization_type::hf26 );

  std::cout<<"Public keys from signatures"<<std::endl;
  for( auto& public_key : _public_keys )
  {
    std::cout<<( (std::string)public_key )<<std::endl;
  }

  required_authorities_type _required_authorities;
  _required_authorities.other.emplace_back( _auth );

  try
  {
    hive::protocol::verify_authority( _required_authorities,
                        _public_keys,
                        [&]( std::string account_name ){ return authority(); },
                        [&]( std::string account_name ){ return authority(); },
                        [&]( std::string account_name ){ return authority(); },
                        [&]( std::string witness_name ){ return public_key_type(); },
                        HIVE_MAX_SIG_CHECK_DEPTH,
                        HIVE_MAX_AUTHORITY_MEMBERSHIP,
                        HIVE_MAX_SIG_CHECK_ACCOUNTS,
                        false,
                        flat_set<account_name_type>(),
                        flat_set<account_name_type>(),
                        flat_set<account_name_type>());
    auto _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
    std::cout<<"verifying: 0"<<" time: "<<_interval<<"[us]"<<std::endl;
  }
  catch(...)
  {
    std::cout<<"FAIL..."<<std::endl;
    return 0;
  }

  return 0;
}
