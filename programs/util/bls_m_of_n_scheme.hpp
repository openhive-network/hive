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
    if( !( nr_signers <= nr_signatures && nr_signers > 0 ) )
      throw std::runtime_error("Incorrect values...");

    make_keys();
  }

  std::vector<uint8_t> merge_pk_message( const G1Element& _public_key, const std::vector<uint8_t>& _message )
  {
    bls::Bytes message = bls::Bytes( _message );
    std::vector<uint8_t> augMessage = _public_key.Serialize();

    augMessage.reserve(augMessage.size() + message.size());
    augMessage.insert(augMessage.end(), message.begin(), message.end());

    return augMessage;
  };

  std::vector<uint8_t> merge_pk_number( const G1Element& _public_key, const uint8_t& _number )
  {
    return merge_pk_message( _public_key, { _number } );
  };

  G1Element aggregate( const std::vector<G1Element>& src_public_keys )
  {
    G1Element _result;

    for( auto& item : src_public_keys )
      _result += item;

    return _result;
  }

  std::vector<G2Element> create_membership_keys( const std::vector<PrivateKey>& src_private_keys, const G1Element& aggregated_public_key )
  {
    std::vector<G2Element> _result;

    auto _size = src_private_keys.size();

    for( size_t i = 0; i < _size; ++i )
    {
      G2Element _sum_keys;
      for( size_t k = 1; k < _size; k += 2 )
      {
        std::vector<uint8_t> _val{ static_cast<uint8_t>( i ) };
        _sum_keys += AugSchemeMPL().Sign( src_private_keys[k], _val, aggregated_public_key );
      }
      for( size_t k = 0; k < _size; k += 2 )
      {
        std::vector<uint8_t> _val{ static_cast<uint8_t>( i ) };
        _sum_keys += AugSchemeMPL().Sign( src_private_keys[k], _val, aggregated_public_key );
      }
      _result.emplace_back( _sum_keys );
    }

    return _result;
  }

  void check_membership_keys( const std::vector<G2Element>& membership_keys, const G1Element& aggregated_public_key )
  {
    size_t _counter = 0;
    for( size_t i = 0; i < membership_keys.size(); ++i )
    {
      std::vector<uint8_t> _tmp{ static_cast<uint8_t>( i ) };
      if( AugSchemeMPL().AggregateVerify( { aggregated_public_key }, std::vector<std::vector<uint8_t>>{ _tmp }, membership_keys[i] ) )
        ++_counter;
    }
    std::cout<<"membership checker: "<<_counter<<"/"<<membership_keys.size()<<" passed"<<std::endl;
  }

  std::vector<G2Element> sign( const std::vector<PrivateKey>& src_private_keys, const std::vector<G2Element>& membership_keys, const G1Element& aggregated_public_key )
  {
    std::vector<G2Element> _result;

    for( size_t i = 0; i < nr_signers; i += 2 )
    {
      G2Element _temp_sig = AugSchemeMPL().Sign( src_private_keys[i], content, aggregated_public_key );
      _temp_sig += membership_keys[i];
      _result.emplace_back( _temp_sig );
    }
    for( size_t i = 1; i < nr_signers; i += 2 )
    {
      G2Element _temp_sig = AugSchemeMPL().Sign( src_private_keys[i], content, aggregated_public_key );
      _temp_sig += membership_keys[i];
      _result.emplace_back( _temp_sig );
    }

    return _result;
  }

  std::pair<G1Element, G2Element> create_partial_signature_and_public_key( const std::vector<G1Element>& src_public_keys, const std::vector<G2Element>& signatures )
  {
    std::pair<G1Element, G2Element> _result;

    for( size_t i = 0; i < nr_signers; ++i )
    {
      _result.first += src_public_keys[i];
      _result.second += signatures[i];
    }

    return _result;
  }

  bool verify( const std::pair<G1Element, G2Element>& partial_signature_and_public_key, const G1Element& aggregated_public_key )
  {
    start_time();
    /*
      Final verification. Here an example for 2 signatures.
      e(G, S’) = e(P’, H(P, m))⋅e(P, H(P, 1)+H(P, 3))
    */
    vector<G1Element> _verify_pub_keys;
    vector<vector<uint8_t>> _verify_message;

    //e(P’, H(P, m))
    _verify_pub_keys.push_back( partial_signature_and_public_key.first );
    _verify_message.push_back( merge_pk_message( aggregated_public_key, content ) );

    //e(P, H(P, 1)+H(P, 3))
    for( size_t i = 0; i < nr_signers; ++i )
    {
      _verify_pub_keys.push_back( aggregated_public_key );
      _verify_message.push_back( merge_pk_number( aggregated_public_key, i ) );
    }

    assert( _verify_pub_keys.size() == _verify_message.size() );
    assert( _verify_pub_keys.size() == nr_signers + 1 );

    end_time( "verify: prepare params" );
    //bool _result = AugSchemeMPL().AggregateVerify( _verify_pub_keys, _verify_message, _final_signature );
    start_time();
    bool _result = CoreMPL(AugSchemeMPL::CIPHERSUITE_ID).AggregateVerify( _verify_pub_keys, _verify_message, partial_signature_and_public_key.second );
    end_time( "verify" );
    return _result;
  }

  void run()
  {
    start_time();
    G1Element _aggregated_public_key = aggregate( public_keys );
    end_time( "aggregate" );

    start_time();
    std::vector<G2Element> _membership_keys = create_membership_keys( private_keys, _aggregated_public_key );
    end_time( "create_membership_keys" );

    start_time();
    check_membership_keys( _membership_keys, _aggregated_public_key );
    end_time( "check_membership_keys" );

    start_time();
    std::vector<G2Element> _signatures = sign( private_keys, _membership_keys, _aggregated_public_key );
    end_time( "sign" );

    start_time();
    std::pair<G1Element, G2Element> _partial_signature_and_public_key = create_partial_signature_and_public_key( public_keys, _signatures );
    end_time( "create_partial_signature_and_public_key" );

    start_time();
    bool _verification = verify( _partial_signature_and_public_key, _aggregated_public_key );

    std::cout<<"***** N-of-M: "<<_verification<<" *****"<<std::endl;
  }

};
