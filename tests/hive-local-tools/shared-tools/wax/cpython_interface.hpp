#pragma once

#include<string>
#include<functional>

namespace cpp
{
  enum error_code { fail, ok };

  struct result
  {
    error_code value = error_code::fail;

    std::string content;
    std::string exception_message;
  };

  class protocol
  {
    private:

      using callback = std::function<void(result&)>;

      result method_wrapper( callback&& method );

    public:

      result cpp_validate_operation( const std::string& operation );
      result cpp_validate_transaction( const std::string& transaction );
      result cpp_calculate_digest( const std::string& transaction, const std::string& chain_id );
      result cpp_serialize_transaction( const std::string& transaction );
  };
}
