
#include <iostream>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <fc/io/json.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/variant.hpp>

#include <hive/utilities/key_conversion.hpp>

#include <hive/protocol/crypto.hpp>

namespace bpo = boost::program_options;

struct tx_signing_request
{
  hive::protocol::transaction     tx;
  std::string                     wif;
};

struct tx_signing_result
{
  hive::protocol::transaction     tx;
  fc::sha256                      digest;
  fc::sha256                      sig_digest;
  hive::protocol::public_key_type key;
  hive::protocol::signature_type  sig;
};

struct error_result
{
  std::string error;
};

FC_REFLECT( tx_signing_request, (tx)(wif) )
FC_REFLECT( tx_signing_result, (digest)(sig_digest)(key)(sig) )
FC_REFLECT( error_result, (error) )

int main(int argc, char** argv, char** envp)
{
  boost::program_options::options_description opts;

  opts.add_options()
  ("chain-id", bpo::value< std::string >()->default_value( HIVE_CHAIN_ID ), "Chain ID to connect to")
  ("sign-type", bpo::value< std::string >()->default_value( "legacy" ), "Allows to sign a transaction using legacy/HF26 format. Possible values(legacy/hf26). By default is `legacy`." )
  ;

  bpo::variables_map options;

  bpo::store( bpo::parse_command_line(argc, argv, opts), options );

  //********** parsing `chain-id` **********
  hive::protocol::chain_id_type chainId = HIVE_CHAIN_ID;
  auto chain_id_str = options.at("chain-id").as< std::string >();

  try
  {
    chainId = hive::protocol::chain_id_type( chain_id_str );
  }
  catch( fc::exception& )
  {
    std::cerr << "Could not parse chain-id as hex string. Chain ID String: `" << chain_id_str << "'" << std::endl;
    std::cerr << "Option ignored, default chain-id used." << std::endl;
  }

  //********** parsing `sign-transaction` **********
  hive::protocol::pack_type _pack = hive::protocol::pack_type::legacy;
  auto _sign_type_str = options["sign-type"].as<std::string>();
  try
  {
    fc::variant _v = _sign_type_str;
    from_variant( _v, _pack );
  }
  catch( fc::exception& )
  {
    std::cerr << "Could not parse sign-transaction as a pack type. Sign Type String: `" << _sign_type_str << "'" << std::endl;
    std::cerr << "Option ignored, default sign-type used." << std::endl;
  }

  // hash key pairs on stdin
  std::string hash, wif;
  while( std::cin )
  {
    std::string line;
    std::getline( std::cin, line );
    boost::trim(line);
    if( line == "" )
      continue;

    try
    {
      fc::variant v = fc::json::from_string( line, fc::json::strict_parser );
      tx_signing_request sreq;
      fc::from_variant( v, sreq );

      tx_signing_result sres;
      sres.tx = sreq.tx;
      sres.digest = sreq.tx.digest();
      sres.sig_digest = sig_digest(sreq.tx, chainId, _pack);

      auto priv_key = hive::utilities::wif_to_key( sreq.wif );

      if(priv_key)
      {
        sres.sig = priv_key->sign_compact( sres.sig_digest );
        sres.key = hive::protocol::public_key_type( priv_key->get_public_key() );
        std::string sres_str = fc::json::to_string( sres );
        std::cout << "{\"result\":" << sres_str << "}" << std::endl;
      }
      else
      {
        if(sreq.wif.empty())
          std::cerr << "Missing Wallet Import Format in the input JSON structure" << std::endl;
        else
          std::cerr << "Passed JSON points to invalid data according to Wallet Import Format specification: `" << sreq.wif << "'" << std::endl;
      }
    }
    catch( const fc::exception& e )
    {
      error_result eresult;
      eresult.error = e.to_detail_string();
      std::cout << fc::json::to_string( eresult ) << std::endl;
    }
  }
  return 0;
}
