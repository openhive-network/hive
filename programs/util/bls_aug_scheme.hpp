#pragma once

#include "bls_scheme.hpp"

#include<string>
#include<vector>

using std::string;
using std::vector;

using namespace bls;

struct aug_scheme: public scheme
{
  void make_keys()
  {
    for( size_t i = 0; i < nr_signatures; ++i )
    {
      PrivateKey _private_key = AugSchemeMPL().KeyGen( get_seed(  i ) );

      public_keys.emplace_back( _private_key.GetG1Element() );
      private_keys.emplace_back( _private_key );
    }
  }

  aug_scheme( uint32_t nr_signatures ) : scheme( nr_signatures )
  {
    make_keys();
  }


  void sign_content( const std::vector<size_t>& idxs )
  {
    for( size_t i = 0; i < idxs.size(); ++i )
    {
      signatures.emplace_back( AugSchemeMPL().Sign( private_keys[ idxs[i] ], content ) );
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
    aggregated_signature = AugSchemeMPL().Aggregate( signatures );
  }

  void aggregate_signatures( const std::vector<G2Element>& a, const std::vector<G2Element>& b)
  {
    std::vector<G2Element> _res{ a.begin(), a.end() };
    std::copy( b.begin(), b.end(), std::back_inserter( _res ) );
    aggregated_signature = AugSchemeMPL().Aggregate( _res );
  }

  bool verify_impl( const std::vector<G1Element>& _public_keys )
  {
    vector<vector<uint8_t>> _content;

    for( size_t i = 0; i < _public_keys.size(); ++i )
      _content.push_back( content );

    std::cout<<"asig: "<<Util::HexStr( aggregated_signature.Serialize() )<<std::endl;

    //std::cout<<std::endl;

    // for(auto& pk : _public_keys )
    //   std::cout<<"pk  : "<<Util::HexStr( pk.Serialize() )<<std::endl;

    //std::cout<<std::endl;

    //for(auto& c : _content )
      //std::cout<<"cont: "<<Util::HexStr( c )<<std::endl;

    return AugSchemeMPL().AggregateVerify( _public_keys, _content, aggregated_signature );
  }

  bool verify( const std::vector<G1Element>& _public_keys )
  {
    bool _result = verify_impl( _public_keys );

    //*****only 1
    std::vector<G1Element> _public_keys_1{ *_public_keys.begin() };
    bool _result_1 = verify_impl( _public_keys_1 );
    std::cout<<"*****1: "<<_result_1<<std::endl;

    //*****all but one
    std::vector<G1Element> _public_keys_abo{ _public_keys.begin(), _public_keys.end() };
    auto _it = _public_keys_abo.end();
    --_it;
    _public_keys_abo.erase( _it );
    bool _result_abo = verify_impl( _public_keys_abo );
    std::cout<<"*****_result_abo: "<<_result_abo<<std::endl;

    return _result;
  }

};