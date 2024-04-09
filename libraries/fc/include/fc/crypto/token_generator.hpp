#pragma once

#include <string>

namespace fc {

struct token_generator
{
  static std::string generate_token( const std::string& salt, unsigned int length );
  static uint32_t generate_nonce( const std::string& seed );
};

};
