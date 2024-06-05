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

  bool create_private_key_share( size_t num_signed, size_t num_all )
  {
    std::vector<size_t> participants(num_all);
    for (size_t i = 0; i < num_all; ++i) participants.at(i) = i + 1; //set participants indices 1,2,3

    libff::alt_bn128_G2 common_public = libff::alt_bn128_G2::zero();

    std::vector< std::vector< libff::alt_bn128_Fr > > secret_shares_all;

    //participant_0
    // DKGBLSWrapper dkg_wrap_0( num_signed, num_all );
    // std::shared_ptr< std::vector< libff::alt_bn128_Fr > > secret_shares_all_0 = dkg_wrap_0.createDKGSecretShares();
    // std::shared_ptr< std::vector< libff::alt_bn128_G2 > > public_shares_ptr_0 = dkg_wrap_0.createDKGPublicShares();
    // common_public = common_public + public_shares_ptr_0->at( 0 );

    //participant_1
    // DKGBLSWrapper dkg_wrap_1( num_signed, num_all );
    // std::shared_ptr< std::vector< libff::alt_bn128_Fr > > secret_shares_all_1 = dkg_wrap_1.createDKGSecretShares();
    // std::shared_ptr< std::vector< libff::alt_bn128_G2 > > public_shares_ptr_1 = dkg_wrap_1.createDKGPublicShares();
    // common_public = common_public + public_shares_ptr_1->at( 0 );

    //participant_2
    // DKGBLSWrapper dkg_wrap_2( num_signed, num_all );
    // std::shared_ptr< std::vector< libff::alt_bn128_Fr > > secret_shares_all_2 = dkg_wrap_2.createDKGSecretShares();
    // std::shared_ptr< std::vector< libff::alt_bn128_G2 > > public_shares_ptr_2 = dkg_wrap_2.createDKGPublicShares();
    // common_public = common_public + public_shares_ptr_2->at( 0 );

    for ( size_t i = 0; i < num_all; i++ )
    {
        DKGBLSWrapper dkg_wrap( num_signed, num_all );

        std::shared_ptr< std::vector< libff::alt_bn128_Fr > > secret_shares_ptr = dkg_wrap.createDKGSecretShares();
        secret_shares_all.push_back( *secret_shares_ptr );

        std::shared_ptr< std::vector< libff::alt_bn128_G2 > > public_shares_ptr = dkg_wrap.createDKGPublicShares();
        common_public = common_public + public_shares_ptr->at( 0 );
    }

    std::vector< std::vector< libff::alt_bn128_Fr > > secret_key_shares;

    std::vector< libff::alt_bn128_Fr > secret_key_contribution;

    /*
      participant_0
        got from participant_1 number "0"
        got from participant_2 number "0"
    */
    // secret_key_contribution.push_back( (*secret_shares_all_0)[0] );
    // secret_key_contribution.push_back( (*secret_shares_all_1)[0] );
    // secret_key_contribution.push_back( (*secret_shares_all_2)[0] );
    // secret_key_shares.push_back( secret_key_contribution );
    // secret_key_contribution.clear();

    /*
      participant_1
        got from participant_0 number "1"
        got from participant_2 number "1"
    */
    // secret_key_contribution.push_back( (*secret_shares_all_0)[1] );
    // secret_key_contribution.push_back( (*secret_shares_all_1)[1] );
    // secret_key_contribution.push_back( (*secret_shares_all_2)[1] );
    // secret_key_shares.push_back( secret_key_contribution );
    // secret_key_contribution.clear();

    /*
      participant_2
        got from participant_0 number "2"
        got from participant_1 number "2"
    */
    // secret_key_contribution.push_back( (*secret_shares_all_0)[2] );
    // secret_key_contribution.push_back( (*secret_shares_all_1)[2] );
    // secret_key_contribution.push_back( (*secret_shares_all_2)[2] );
    // secret_key_shares.push_back( secret_key_contribution );
    // secret_key_contribution.clear();

    for ( size_t i = 0; i < num_all; i++ )
    {
        std::vector< libff::alt_bn128_Fr > secret_key_contribution;
        for ( size_t j = 0; j < num_all; j++ )
        {
            secret_key_contribution.push_back( secret_shares_all.at( j ).at( i ) );
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

    auto _content = create_content();

    start_time();
    BLSSigShareSet sigSet(num_signed, num_all);

    for (size_t i = 0; i < num_signed; ++i) {
      std::shared_ptr<BLSPrivateKeyShare> skey = _private_keys->at(i);

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
    bool _result = _common_public_key.VerifySig( _content, common_sig_ptr );
    end_time( "verify" );
    return _result;
  }

  bool run( size_t num_all, size_t num_signed )
  {
    return create_private_key_share( num_signed, num_all );
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
