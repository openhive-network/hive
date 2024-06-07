#pragma once

#include "bls_scheme.hpp"

#include<string>
#include<vector>

using std::string;
using std::vector;

using namespace bls;

struct pop_scheme: public scheme
{
  void make_keys()
  {
    for( size_t i = 0; i < nr_signatures; ++i )
    {
      PrivateKey _private_key = PopSchemeMPL().KeyGen( get_seed( 1 ) );

      public_keys.emplace_back( _private_key.GetG1Element() );
      private_keys.emplace_back( _private_key );
    }
  }

  pop_scheme( uint32_t nr_signatures ) : scheme( nr_signatures )
  {
    make_keys();
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

  std::vector<G2Element> sign( const std::vector<PrivateKey>& src_private_keys )
  {
    std::vector<G2Element> _result;
    for( auto& private_key : src_private_keys )
    {
      _result.emplace_back( PopSchemeMPL().Sign( private_key, content ) );
    }
    return _result;
  }

  std::vector<G2Element>  sign_content_2()
  {
    return sign( private_keys );
  }

  G2Element aggregate_signatures( std::vector<G2Element>& src_signatures )
  {
    return PopSchemeMPL().Aggregate( src_signatures );
  }

  void aggregate_signatures( const std::vector<G2Element>& a, const std::vector<G2Element>& b)
  {
    std::vector<G2Element> _res{ a.begin(), a.end() };
    std::copy( b.begin(), b.end(), std::back_inserter( _res ) );
    aggregated_signature = PopSchemeMPL().Aggregate( _res );
  }

  bool verify( const std::vector<G1Element>& src_public_keys, const G2Element& aggregated_signature )
  {
    return PopSchemeMPL().FastAggregateVerify( src_public_keys, content, aggregated_signature );
  }

};