#include <fc/crypto/token_generator.hpp>

#include <random>
#include <sstream>

namespace fc {

std::string token_generator::generate_token( const std::string& salt, unsigned int length )
{
    std::random_device rd;
    std::string _str_seed = salt + std::to_string( rd() );
    std::seed_seq seq( _str_seed.begin(), _str_seed.end() );

    std::mt19937 gen;
    gen.seed( seq );

    std::uniform_int_distribution<> dis(0, 255);

    std::stringstream ss;
    for( unsigned int i = 0; i < length; i++ )
    {
        auto rc = static_cast<unsigned char>( dis( gen ) );

        std::stringstream hexstream;
        hexstream << std::hex << int(rc);
        auto hex = hexstream.str(); 
        ss << (hex.length() < 2 ? '0' + hex : hex);
    }
    return ss.str();
}

uint32_t token_generator::generate_nonce( const std::string& seed )
{
  std::seed_seq _seq( seed.begin(), seed.end() );
  std::vector<uint32_t> _result( 1 );

  _seq.generate( _result.begin(), _result.end() );

  return *( _result.begin() );
}

};
