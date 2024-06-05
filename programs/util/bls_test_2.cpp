#include <bls/BLSPrivateKey.h>
#include <bls/BLSPrivateKeyShare.h>
#include <bls/BLSPublicKey.h>
#include <bls/BLSPublicKeyShare.h>
#include <bls/BLSSigShare.h>
#include <bls/BLSSigShareSet.h>
#include <bls/BLSSignature.h>

#include <dkg/DKGBLSWrapper.h>

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

  using private_key_share_ptr = std::shared_ptr<BLSPrivateKeyShare>;
  using private_key_share_items = std::vector<private_key_share_ptr>;
  using private_key_share_items_ptr = std::shared_ptr<private_key_share_items>;
  using generated_basic_keys = std::pair<private_key_share_items_ptr, BLSPublicKey>;

  void display_fun( int size )
  {
    std::cout<<"size: "<<size<<std::endl;
  }

  void display_fun( int idx, int idx2 )
  {
    std::cout<<"idx: "<<idx<<" idx2: "<<idx2<<std::endl;
  }

  std::shared_ptr<std::array<uint8_t, 32>> create_content()
  {
    start_time();
    std::default_random_engine rand_gen((unsigned int) time(0));
    std::array<uint8_t, 32> hash_byte_arr;
    for ( size_t i = 0; i < 32 ; i++){        //generate random hash
      hash_byte_arr.at(i) = rand_gen() % 255;
    }
    std::shared_ptr<std::array<uint8_t, 32>> hash_ptr = std::make_shared< std::array<uint8_t, 32> >(hash_byte_arr);
    end_time( "create_content" );

    return hash_ptr;
  }

  generated_basic_keys create_private_key_share( size_t num_signed, size_t num_all )
  {
    libff::alt_bn128_G2 common_public = libff::alt_bn128_G2::zero();

    std::vector< std::vector< libff::alt_bn128_Fr > > secret_shares_all;

    for ( size_t i = 0; i < num_all; i++ )
    {
        DKGBLSWrapper dkg_wrap( num_signed, num_all );

        std::shared_ptr< std::vector< libff::alt_bn128_Fr > > secret_shares_ptr = dkg_wrap.createDKGSecretShares();
        secret_shares_all.push_back( *secret_shares_ptr );
        display_fun( secret_shares_ptr->size() );

        std::shared_ptr< std::vector< libff::alt_bn128_G2 > > public_shares_ptr = dkg_wrap.createDKGPublicShares();
        common_public = common_public + public_shares_ptr->at( 0 );
    }

    std::vector< std::vector< libff::alt_bn128_Fr > > secret_key_shares;

    for ( size_t i = 0; i < num_all; i++ )
    {
        std::vector< libff::alt_bn128_Fr > secret_key_contribution;
        for ( size_t j = 0; j < num_all; j++ )
        {
            secret_key_contribution.push_back( secret_shares_all.at( j ).at( i ) );
            display_fun( j, i );
        }
        secret_key_shares.push_back( secret_key_contribution );
    }

    private_key_share_items_ptr _private_keys( std::make_shared<private_key_share_items>() );

    for ( size_t i = 0; i < num_all; i++ )
    {
      DKGBLSWrapper dkg_wrap( num_signed, num_all );
      BLSPrivateKeyShare _tmp = dkg_wrap.CreateBLSPrivateKeyShare( std::make_shared< std::vector< libff::alt_bn128_Fr > >( secret_key_shares.at( i ) ) );
      _private_keys->push_back( std::make_shared<BLSPrivateKeyShare>( std::move( _tmp ) ) );
    }

    BLSPublicKey _common_public_key( common_public );

    return std::make_pair( _private_keys, _common_public_key );
  }

  bool run( size_t num_all, size_t num_signed )
  {
    start_time();
    std::vector<size_t> participants(num_all);
    for (size_t i = 0; i < num_all; ++i) participants.at(i) = i + 1; //set participants indices 1,2,3

    auto _generated_basic_keys = create_private_key_share(num_signed, num_all);
    end_time( "create_private_keys" );

    auto _content = create_content();

    start_time();
    BLSSigShareSet sigSet(num_signed, num_all);

    for (size_t i = 0; i < num_signed; ++i) {
      std::shared_ptr<BLSPrivateKeyShare> skey = _generated_basic_keys.first->at(i);

      // sign with private key of each participant
      std::shared_ptr<BLSSigShare> sigShare = skey->sign( _content, participants.at(i) );

      sigSet.addSigShare(sigShare);
    }
    end_time( "sign" );

    start_time();
    std::shared_ptr<BLSSignature> common_sig_ptr = sigSet.merge();         //create common signature
    end_time( "aggregate signature" );

    // verify common signature with common public key
    start_time();
    bool _result = _generated_basic_keys.second.VerifySig( _content, common_sig_ptr );
    end_time( "verify" );

    return _result;
  }

};


int main(int argc, char* argv[])
{
  try
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
  }
  catch( std::exception& ex )
  {
    std::cout<<ex.what()<<std::endl;
  }
  catch(...)
  {
    std::cout<<"error..."<<std::endl;
  }

  return 0;
}