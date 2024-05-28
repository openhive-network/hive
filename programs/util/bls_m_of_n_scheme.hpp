#pragma once

#include "bls_scheme.hpp"

#include<string>
#include<vector>

using std::string;
using std::vector;

using namespace bls;

struct bls_m_of_n_scheme: public scheme
{
  uint32_t nr_signers = 0;

  void make_keys()
  {
    for( size_t i = 0; i < nr_signatures; ++i )
    {
      PrivateKey _private_key = AugSchemeMPL().KeyGen( get_seed(  i ) );

      public_keys.emplace_back( _private_key.GetG1Element() );
      private_keys.emplace_back( _private_key );
    }
  }

  bls_m_of_n_scheme( uint32_t nr_signers, uint32_t nr_signatures ) : scheme( nr_signatures ), nr_signers( nr_signers )
  {
    assert( nr_signers <= nr_signatures );
    make_keys();
  }

  void run()
  {
    //setup.
    G1Element _aggregated_public_key;

    //Aggregated public key
    for( auto& public_key : public_keys )
    {
      _aggregated_public_key += public_key;
    }

    //membership keys
    std::vector<G2Element> _membership_keys;
    for( size_t i = 0; i < private_keys.size(); ++i )
    {
      G2Element _sum_keys;
      for( size_t k = 0; k < private_keys.size(); ++k )
      {
        std::vector<uint8_t> _val{ static_cast<uint8_t>( i ) };
        _sum_keys += AugSchemeMPL().Sign( private_keys[k], _val, _aggregated_public_key );
      }
      _membership_keys.emplace_back( _sum_keys );
    }

    //only `nr_signers` signers
    std::vector<G2Element> _temp_signatures;
    for( size_t i = 0; i < nr_signers; ++i )
    {
      G2Element _temp_sig = AugSchemeMPL().Sign( private_keys[i], content, _aggregated_public_key );
      _temp_sig += _membership_keys[i];
      _temp_signatures.emplace_back( _temp_sig );
    }

    G1Element _final_public_key;
    G2Element _final_signature;

    for( size_t i = 0; i < nr_signers; ++i )
    {
      _final_signature += _temp_signatures[i];
      _final_public_key += public_keys[i];
    }

    vector<vector<uint8_t>> _content;

    //for( size_t i = 0; i < _public_keys.size(); ++i )
    _content.push_back( content );

    bool _result = AugSchemeMPL().AggregateVerify( { _final_public_key }, _content, _final_signature );
    std::cout<<"n of m: "<<_result<<std::endl;
  }

};