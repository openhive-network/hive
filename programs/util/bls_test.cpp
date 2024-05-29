
#include <chrono>

#include "bls_utility.hpp"

#include <boost/program_options.hpp>

using namespace bls;

void test_pop( uint32_t nr_signatures, uint32_t nr_signatures_2, uint32_t nr_threads )
{
  uint32_t _threads = nr_threads;
  pop_scheme obj_00( nr_signatures );

  auto _start = std::chrono::high_resolution_clock::now();
  obj_00.sign_content();
  auto _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"signing: "<<" time: "<<_interval<<"[us]"<<std::endl;

  _start = std::chrono::high_resolution_clock::now();
  obj_00.aggregate_signatures();
  _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"aggregating: "<<" time: "<<_interval<<"[us]"<<std::endl<<std::endl;

  _start = std::chrono::high_resolution_clock::now();
  bool _result = verify( obj_00, obj_00.public_keys, _threads );
  _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"XXX: verifying: "<<_result<<" time: "<<_interval<<"[us]"<<std::endl<<std::endl;

  ////=============================================
  std::vector<G1Element> _public_keys_00( obj_00.public_keys.begin(), obj_00.public_keys.end() );
  _public_keys_00.erase( _public_keys_00.begin() );
  _public_keys_00.insert( _public_keys_00.begin(), PopSchemeMPL().KeyGen( obj_00.get_seed( 1342343 ) ).GetG1Element() );

  _start = std::chrono::high_resolution_clock::now();
  _result = verify( obj_00, _public_keys_00, _threads );
  _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"000: verifying: "<<_result<<" time: "<<_interval<<"[us]"<<std::endl<<std::endl;

  ////=============================================
  std::vector<G1Element> _public_keys_01( obj_00.public_keys.begin(), obj_00.public_keys.end() );
  auto _it_01 = _public_keys_01.end();
  --_it_01;
  _public_keys_01.erase( _it_01 );
  _public_keys_01.push_back( PopSchemeMPL().KeyGen( obj_00.get_seed( 1342343 ) ).GetG1Element() );

  _start = std::chrono::high_resolution_clock::now();
  _result = verify( obj_00, _public_keys_01, _threads );
  _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"001: verifying: "<<_result<<" time: "<<_interval<<"[us]"<<std::endl<<std::endl;

  ////=============================================
  std::vector<G1Element> _public_keys_02( obj_00.public_keys.rbegin(), obj_00.public_keys.rend() );

  _start = std::chrono::high_resolution_clock::now();
  _result = verify( obj_00, _public_keys_02, _threads );
  _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"002: verifying: "<<_result<<" time: "<<_interval<<"[us]"<<std::endl<<std::endl;

  ////=============================================
  std::vector<G1Element> _public_keys_03( obj_00.public_keys.rbegin(), obj_00.public_keys.rend() );

  auto _middle = _public_keys_03.size() / 2;
  auto _it_03 = _public_keys_03.begin() + _middle;
  _public_keys_03.push_back( *_it_03 );

  _it_03 = _public_keys_03.begin() + _middle;
  _public_keys_03.erase( _it_03 );

  _start = std::chrono::high_resolution_clock::now();
  _result = verify( obj_00, _public_keys_03, _threads );
  _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"003: verifying: "<<_result<<" time: "<<_interval<<"[us]"<<std::endl<<std::endl;

  ////=============================================
  std::vector<G1Element> _public_keys_04( obj_00.public_keys.rbegin(), obj_00.public_keys.rend() );

  auto _middle_04 = _public_keys_04.size() / 2;

  auto _it_04 = _public_keys_04.begin() + _middle_04;
  _public_keys_04.erase( _it_04 );

  _start = std::chrono::high_resolution_clock::now();
  _result = verify( obj_00, _public_keys_04, _threads );
  _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"004: verifying: "<<_result<<" time: "<<_interval<<"[us]"<<std::endl<<std::endl;

  ////=============================================
  std::vector<G1Element> _public_keys_05( obj_00.public_keys.rbegin(), obj_00.public_keys.rend() );
  _public_keys_05.erase( _public_keys_05.begin() );

  _start = std::chrono::high_resolution_clock::now();
  _result = verify( obj_00, _public_keys_05, _threads );
  _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"005: verifying: "<<_result<<" time: "<<_interval<<"[us]"<<std::endl<<std::endl;

  ////=============================================
  pop_scheme obj_01( nr_signatures_2 );

  _start = std::chrono::high_resolution_clock::now();
  obj_01.sign_content();
  _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"signing2: "<<" time: "<<_interval<<"[us]"<<std::endl;

  _start = std::chrono::high_resolution_clock::now();
  obj_01.aggregate_signatures( { obj_00.aggregated_signature }, obj_01.signatures );
  _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"aggregating2: "<<" time: "<<_interval<<"[us]"<<std::endl;

  ////=============================================
  std::vector<G1Element> _public_keys_100( obj_00.public_keys.begin(), obj_00.public_keys.end() );
  std::copy( obj_01.public_keys.begin(), obj_01.public_keys.end(), std::back_inserter( _public_keys_100 ) );

  _start = std::chrono::high_resolution_clock::now();
  _result = verify( obj_01, _public_keys_100, _threads );
  _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"100: verifying2: "<<_result<<" time: "<<_interval<<"[us]"<<std::endl<<std::endl;
}

void test_aug( uint32_t nr_signatures, uint32_t nr_signatures_2, uint32_t nr_threads )
{
  uint32_t _threads = nr_threads;
  aug_scheme obj_00( nr_signatures );

  auto _start = std::chrono::high_resolution_clock::now();
  obj_00.sign_content();
  auto _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"signing: "<<" time: "<<_interval<<"[us]"<<std::endl;

  _start = std::chrono::high_resolution_clock::now();
  obj_00.aggregate_signatures();
  _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"aggregating: "<<" time: "<<_interval<<"[us]"<<std::endl<<std::endl;

  _start = std::chrono::high_resolution_clock::now();
  bool _result = verify( obj_00, obj_00.public_keys, _threads );
  _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
  std::cout<<"XXX: verifying: "<<_result<<" time: "<<_interval<<"[us]"<<std::endl<<std::endl;
}

void test_n_of_m( uint32_t nr_signatures, uint32_t nr_signers )
{
  bls_m_of_n_scheme _obj( nr_signers, nr_signatures );
  _obj.run();
}

int main(int argc, char* argv[])
{
  try
  {
    namespace bpo = boost::program_options;
    using bpo::options_description;
    using bpo::variables_map;

    bpo::options_description opts{""};

    opts.add_options()("nr-threads", bpo::value< uint32_t >()->default_value( 1 ), "Number of signatures");
    opts.add_options()("nr-signatures", bpo::value< uint32_t >()->default_value( 1 ), "Number of signatures");
    opts.add_options()("nr-signatures-2", bpo::value< uint32_t >()->default_value( 1 ), "Number of signatures2");
    opts.add_options()("nr-signers", bpo::value< uint32_t >()->default_value( 1 ), "Number of signers");
    boost::program_options::variables_map options_map;
    boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(opts).run(), options_map);

    //test_pop( options_map["nr-signatures"].as<uint32_t>(), options_map["nr-signatures-2"].as<uint32_t>(), options_map["nr-threads"].as<uint32_t>() );
    //test_aug( options_map["nr-signatures"].as<uint32_t>(), options_map["nr-signatures-2"].as<uint32_t>(), options_map["nr-threads"].as<uint32_t>() );
    test_n_of_m( options_map["nr-signatures"].as<uint32_t>(), options_map["nr-signers"].as<uint32_t>() );
  }
  catch( std::exception& ex )
  {
    std::cout<<ex.what()<<std::endl;
  }
  catch(...)
  {
    std::cout<<"error..."<<std::endl;
  }
}
