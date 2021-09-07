#pragma once

#include <hive/protocol/operations.hpp>

#include <fc/io/sstream.hpp>
#include <fc/crypto/ripemd160.hpp>

#include <string>

namespace hive::plugins::sql_serializer {

  struct data2_sql_tuple_base
    {
    using signature_type = hive::protocol::signature_type;
    data2_sql_tuple_base() = default;

    protected:
      std::string escape(const std::string& source) const;
      std::string escape_raw(const fc::ripemd160& hash) const;
      std::string escape_raw(const fc::optional<signature_type>& sign) const;

    private:
      fc::string escape_sql(const std::string &text) const;
    };

} // namespace hive::plugins::sql_serializer

