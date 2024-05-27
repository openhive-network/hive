#pragma once

#include<string>
#include<vector>

#include "bls.hpp"
#include "test-utils.hpp"

using std::string;
using std::vector;

using namespace bls;

struct scheme
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

  scheme( uint32_t nr_signatures ) : nr_signatures( nr_signatures )
  {
    content = generate_content( 667 );
  }

};