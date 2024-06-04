#include <bls/BLSPrivateKey.h>
#include <bls/BLSPrivateKeyShare.h>
#include <bls/BLSPublicKey.h>
#include <bls/BLSPublicKeyShare.h>
#include <bls/BLSSigShare.h>
#include <bls/BLSSigShareSet.h>
#include <bls/BLSSignature.h>
//#include <tools/utils.h>

#include<chrono>
#include <boost/program_options.hpp>

struct lib_bls
{
  std::chrono::time_point<std::chrono::high_resolution_clock> time_start;

  void start_time()
  {
    time_start = std::chrono::high_resolution_clock::now();
  }

  void end_time( const std::string& message )
  {
    auto _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - time_start ).count();
    std::cout<<message<<std::endl;
    std::cout<<_interval<<"[us]"<<std::endl;
    std::cout<<( _interval / 1'000 )<<"[ms]"<<std::endl<<std::endl;
  }

  void show_private_keys( const std::shared_ptr<std::vector<std::shared_ptr<BLSPrivateKeyShare>>>& keys )
  {
    auto _keys = *keys;
    for( auto& item : _keys )
    {
      std::shared_ptr< std::string > _str =  item->toString();
      std::cout<<( *_str )<<std::endl;
    }
  }

  bool run( size_t num_all, size_t num_signed )
  {
    start_time();
    std::vector<size_t> participants(num_all);
    for (size_t i = 0; i < num_all; ++i) participants.at(i) = i + 1; //set participants indices 1,2,3

    std::shared_ptr<std::vector<std::shared_ptr<BLSPrivateKeyShare>>> Skeys = BLSPrivateKeyShare::generateSampleKeys(num_signed, num_all)->first;
    end_time( "create_private_keys" );

    //show_private_keys( Skeys );

    start_time();
    std::default_random_engine rand_gen((unsigned int) time(0));
    std::array<uint8_t, 32> hash_byte_arr;
    for ( size_t i = 0; i < 32 ; i++){        //generate random hash
      hash_byte_arr.at(i) = rand_gen() % 255;
    }
    std::shared_ptr<std::array<uint8_t, 32>> hash_ptr = std::make_shared< std::array<uint8_t, 32> >(hash_byte_arr);
    end_time( "create_content" );

    start_time();
    BLSSigShareSet sigSet(num_signed, num_all);

    for (size_t i = 0; i < num_signed; ++i) {
      std::shared_ptr<BLSPrivateKeyShare> skey = Skeys->at(i);

      // sign with private key of each participant
      std::shared_ptr<BLSSigShare> sigShare = skey->sign(hash_ptr, participants.at(i));

      sigSet.addSigShare(sigShare);
    }
    end_time( "sign" );

    start_time();
    std::shared_ptr<BLSSignature> common_sig_ptr = sigSet.merge();         //create common signature
    end_time( "aggregate signature" );

    //create common private key from private keys of each participant
    start_time();
    BLSPrivateKey common_skey(Skeys, std::make_shared<std::vector<size_t>>(participants), num_signed, num_all);
    end_time( "create_common_private_key" );

    //create common public key from common private key
    start_time();
    BLSPublicKey common_pkey(*(common_skey.getPrivateKey()), num_signed, num_all);
    end_time( "create_common_public_key" );

    // verify common signature with common public key
    start_time();
    bool _result = common_pkey.VerifySig(hash_ptr, common_sig_ptr);
    end_time( "verify" );

    return _result;
  }

};


int main(int argc, char* argv[])
{
  namespace bpo = boost::program_options;
  using bpo::options_description;
  using bpo::variables_map;

  bpo::options_description opts{""};

  opts.add_options()("nr-signatures", bpo::value< uint32_t >()->default_value( 1 ), "Number of signatures");
  opts.add_options()("nr-signers", bpo::value< uint32_t >()->default_value( 1 ), "Number of signers");
  boost::program_options::variables_map options_map;
  boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(opts).run(), options_map);

  lib_bls obj;

  bool _verification = obj.run( options_map["nr-signatures"].as<uint32_t>()/*num_all*/, options_map["nr-signers"].as<uint32_t>()/*num_signed*/ );
  std::cout<<"***** N-of-M: "<<_verification<<" *****"<<std::endl;

  return 0;
}