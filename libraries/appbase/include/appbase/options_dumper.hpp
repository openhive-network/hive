#pragma once
#include <boost/program_options.hpp>
#include <fc/variant.hpp>

#include <initializer_list>
#include <optional>
#include <unordered_map>

namespace appbase {

  class options_dumper
  {

    public:

      using bpo     = boost::program_options::options_description;
      using entry_t = std::pair<const std::string, bpo>;

      struct value_info {
        bool        required    = false;
        bool        multitoken  = false;
        bool        composed    = false;
        std::string value_type;
        fc::variant default_value;
      };

      struct option_entry {
        std::string               name;
        std::string               description;
        std::optional<value_info> value;
      };

    private:

      std::unordered_map<std::string, bpo> option_groups;

      std::optional<value_info> get_value_info( const boost::shared_ptr<const boost::program_options::value_semantic>& semantic ) const;
      option_entry serialize_option( const boost::program_options::option_description& option ) const;

    public:

      options_dumper( std::initializer_list<entry_t> entries );

      std::string dump_to_string() const;
  };

};
