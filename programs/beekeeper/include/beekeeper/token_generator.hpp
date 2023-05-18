#pragma once

#include <string>

namespace beekeeper {

struct token_generator
{
  static std::string generate_token( const std::string& salt, unsigned int length );
};

};
