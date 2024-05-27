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

  bool verify( const std::vector<G1Element>& _public_keys )
  {
    vector<vector<uint8_t>> _content;

    _content.push_back( content );

    std::cout<<"asig: "<<Util::HexStr( aggregated_signature.Serialize() )<<std::endl;

    std::cout<<std::endl;

    for(auto& pk : _public_keys )
      std::cout<<"pk  : "<<Util::HexStr( pk.Serialize() )<<std::endl;

    std::cout<<std::endl;

    for(auto& c : _content )
      std::cout<<"cont: "<<Util::HexStr( c )<<std::endl;

    return AugSchemeMPL().AggregateVerify( {_public_keys}, _content, aggregated_signature );
  }

};