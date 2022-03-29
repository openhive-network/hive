#pragma once

#include <fc/string.hpp>

#include<map>

namespace type_extractor
{
  using fc::string;

  class operation_extractor
  {
    public:

      using operation_ids_container_t     = std::map<string, int64_t>;
      using operation_details_container_t = std::map<int64_t, std::pair<string, bool>>;

    private:

      operation_ids_container_t     operations_ids;
      operation_details_container_t operations_details;

    public:

      operation_extractor();

      const operation_ids_container_t& get_operation_ids() const;
      const operation_details_container_t& get_operation_details() const;
  };

} // namespace type_extractor
