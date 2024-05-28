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

    //===============================================
    vector<G1Element> _verify_pub_keys;
    vector<vector<uint8_t>> _verify_message;


    /*
        e(G, S) =
        e(G, S1+S2+…+S1000) =
        e(G, S1)⋅e(G, S2)⋅…⋅e(G, S1000) =
        e(G, pk1×H(m1))⋅…⋅e(G, pk1000×H(m1000)) =
        e(pk1×G, H(m1))⋅…⋅e(pk1000×G, H(m1000)) =
        e(P1, H(m1))⋅e(P2, H(m2))⋅…⋅e(P1000, H(m1000))
    */

    /*
      e(G, S’) =
      e(G, S1+S3)=
      e(G, pk1×H(P, m)+pk3×H(P, m)+MK1+MK3) =
      e(G, pk1×H(P, m)+pk3×H(P, m))⋅e(G, MK1+MK3)=
      e(pk1×G+pk3×G, H(P, m))⋅e(P, H(P, 1)+H(P, 3))=
      e(P’, H(P, m))⋅e(P, H(P, 1)+H(P, 3))
    */

    auto _merge_pk_message = []( const G1Element& _public_key, const std::vector<uint8_t>& _message )
    {
      bls::Bytes message = bls::Bytes( _message );
      std::vector<uint8_t> augMessage = _public_key.Serialize();

      augMessage.reserve(augMessage.size() + message.size());
      augMessage.insert(augMessage.end(), message.begin(), message.end());

      return augMessage;
    };

    auto _merge_pk_message_2 = [&]( const G1Element& _public_key, const uint8_t& _number )
    {
      return _merge_pk_message( _public_key, { _number } );
    };

    //e(P’, H(P, m))
    _verify_pub_keys.push_back( _final_public_key );
    _verify_message.push_back( _merge_pk_message( _aggregated_public_key, content ) );

    //e(P, H(P, 1)+H(P, 3))
    _verify_pub_keys.push_back( _aggregated_public_key );
    vector<uint8_t> _sum_messages;
    for( size_t i = 0; i < nr_signers; ++i )
    {
      std::vector<uint8_t> _tmp = _merge_pk_message_2( _aggregated_public_key, i );
      std::copy( _tmp.begin(), _tmp.end(), std::back_inserter( _sum_messages ) );
    }
    _verify_message.push_back( _sum_messages );

    assert( _verify_pub_keys.size() == _verify_message.size() );
    assert( _verify_pub_keys.size() == 2 );

    bool _result = AugSchemeMPL().AggregateVerify( _verify_pub_keys, _verify_message, _final_signature );
    std::cout<<"n of m: "<<_result<<std::endl;
  }

};