#pragma once

#include<string>

namespace cpp
{
  struct result
  {
    unsigned int value = false;
    std::string content;
  };

  class protocol
  {
    public:
      unsigned int cpp_validate_operation( const std::string& operation );
      unsigned int cpp_validate_transaction( const std::string& transaction );
      result cpp_calculate_digest( const std::string& transaction, const std::string& chain_id );
      result cpp_serialize_transaction( const std::string& transaction );
  };
}
