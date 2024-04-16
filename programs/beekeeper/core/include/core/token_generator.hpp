#pragma once

#include <string>
#include <optional>

namespace beekeeper {

struct token_generator
{
  static std::string generate_token( const std::optional<std::string>& salt, unsigned int length );
};

};
