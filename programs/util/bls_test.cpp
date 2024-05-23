// Copyright 2020 Chia Network Inc

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//    http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <chrono>

#include "bls.hpp"
#include "test-utils.hpp"

#include <boost/program_options.hpp>

using std::string;
using std::vector;


#include <list>

#include <thread>
#include <future>

using namespace bls;

struct mario_test
{
  size_t content_size = 200;
  std::vector<uint8_t> content;

  size_t nr_signatures = 0;

  std::vector<PrivateKey> private_keys;

  std::vector<G1Element> public_keys;

  std::vector<G2Element> signatures;

  G2Element aggregated_signature;


  std::vector<uint8_t> get_seed( size_t start )
  {
      uint8_t buf[32];

      for (int i = 0; i < 32; i++)
          buf[i] = start + i;

      std::vector<uint8_t> ret(buf, buf + 32);
      return ret;
  }

  std::vector<uint8_t> generate_content( size_t seed )
  {
    std::vector<uint8_t> _result;
    for( size_t i = 0; i < content_size; ++i )
    {
      _result.emplace_back( ( i + seed ) % 256 );
    }
    return _result;
  }

  void make_keys()
  {
    for( size_t i = 0; i < nr_signatures; ++i )
    {
      PrivateKey _private_key = PopSchemeMPL().KeyGen( get_seed( 1 ) );

      public_keys.emplace_back( _private_key.GetG1Element() );
      private_keys.emplace_back( _private_key );
    }
  }

    mario_test( uint32_t nr_signatures ) : nr_signatures( nr_signatures )
    {
      make_keys();
      content = generate_content( 667 );
    }


  void sign_content( const std::vector<size_t>& idxs )
  {
    for( size_t i = 0; i < idxs.size(); ++i )
    {
      signatures.emplace_back( PopSchemeMPL().Sign( private_keys[ idxs[i] ], content ) );
    }
  }

  void sign_content()
  {
    std::vector<size_t> _idxs( nr_signatures );

    for( size_t i = 0; i < nr_signatures; ++i )
      _idxs[i] = i;

    sign_content( _idxs );
  }

  void aggregate_signatures()
  {
    aggregated_signature = PopSchemeMPL().Aggregate( signatures );
  }

  void aggregate_signatures( const std::vector<G2Element>& a, const std::vector<G2Element>& b)
  {
    std::vector<G2Element> _res{ a.begin(), a.end() };
    std::copy( b.begin(), b.end(), std::back_inserter( _res ) );
    aggregated_signature = PopSchemeMPL().Aggregate( _res );
  }

  bool verify( const std::vector<G1Element>& _public_keys )
  {
    // G1Element _tmp;
    // for( auto& item : _public_keys )
    //   _tmp += item;
    // return PopSchemeMPL().Verify( _tmp, content, aggregated_signature );
    return PopSchemeMPL().FastAggregateVerify( _public_keys, content, aggregated_signature );
  }

};

void sum( std::promise<G1Element>&& p, const std::vector<G1Element>& public_keys, uint32_t begin, uint32_t end )
{
  G1Element _result;

  for( size_t i = begin; i < end; ++i )
    _result += public_keys[i];

  p.set_value( _result );
}

bool verify( mario_test& obj, const std::vector<G1Element>& public_keys, uint32_t nr_threads )
{
  if( nr_threads == 0 )
    return false;

  if( nr_threads == 1 )
  {
    G1Element _pk;

    auto _start = std::chrono::high_resolution_clock::now();

    for( auto& item : public_keys )
      _pk += item;

    auto _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
    std::cout<<"SUM: "<<" time: "<<_interval<<"[us]"<<std::endl;

    _start = std::chrono::high_resolution_clock::now();
    bool _result = obj.verify( { _pk } );
    _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
    std::cout<<"CAL: time: "<<_interval<<"[us]"<<std::endl;

    return _result;
  }
  else
  {
    auto _start = std::chrono::high_resolution_clock::now();
    auto _size = public_keys.size();
    auto _range = _size / nr_threads;

    std::list<std::thread> threads;

    using _promise_type = std::promise<G1Element>;
    using _future_type = std::future<G1Element>;

    std::vector<_promise_type> promises( nr_threads );
    std::list<_future_type> futures;

    for( uint32_t i = 0; i < nr_threads; ++i )
    {
      auto _begin = i * _range;
      auto _end =  ( i == nr_threads - 1 ) ? _size : ( (i + 1) * _range );

      futures.emplace_back( promises[i].get_future() );
      threads.emplace_back( std::thread( sum, std::move( promises[i] ), public_keys, _begin, _end ) );
    }

    for( auto& thread : threads )
      thread.join();

    std::vector<G1Element> _public_keys;

    for( auto& future : futures )
    {
      _public_keys.emplace_back( future.get() );
    }
    auto _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
    std::cout<<"SUM: "<<" time: "<<_interval<<"[us]"<<std::endl;

    _start = std::chrono::high_resolution_clock::now();
    bool _result = obj.verify( _public_keys );
    _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
    std::cout<<"CAL: time: "<<_interval<<"[us]"<<std::endl;

    return _result;
  }
}

int main(int argc, char* argv[])
{
  namespace bpo = boost::program_options;
  using bpo::options_description;
  using bpo::variables_map;

  bpo::options_description opts{""};

  opts.add_options()("nr-threads", bpo::value< uint32_t >()->default_value( 1 ), "Number of signatures");
  opts.add_options()("nr-signatures", bpo::value< uint32_t >()->default_value( 1 ), "Number of signatures");
  opts.add_options()("nr-signatures-2", bpo::value< uint32_t >()->default_value( 1 ), "Number of signatures2");
  boost::program_options::variables_map options_map;
  boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(opts).run(), options_map);

  {
    uint32_t _threads = options_map["nr-threads"].as<uint32_t>();
    mario_test obj_00( options_map["nr-signatures"].as<uint32_t>() );

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
    mario_test obj_01( options_map["nr-signatures-2"].as<uint32_t>() );

    _start = std::chrono::high_resolution_clock::now();
    obj_01.sign_content();
    _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
    std::cout<<"signing2: "<<" time: "<<_interval<<"[us]"<<std::endl;

    _start = std::chrono::high_resolution_clock::now();
    obj_01.aggregate_signatures( { obj_00.aggregated_signature }, obj_01.signatures );
    _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
    std::cout<<"aggregating2: "<<" time: "<<_interval<<"[us]"<<std::endl;

    ////=============================================
    std::vector<G1Element> _public_keys_06( obj_00.public_keys.begin(), obj_00.public_keys.end() );
    std::copy( obj_01.public_keys.begin(), obj_01.public_keys.end(), std::back_inserter( _public_keys_06 ) );

    _start = std::chrono::high_resolution_clock::now();
    _result = verify( obj_01, _public_keys_06, _threads );
    _interval = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - _start ).count();
    std::cout<<"006: verifying2: "<<_result<<" time: "<<_interval<<"[us]"<<std::endl<<std::endl;

  }
}
