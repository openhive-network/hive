#pragma once
#include <boost/program_options.hpp>
#include <fc/variant.hpp>

#include <initializer_list>
#include <optional>
#include <unordered_map>

namespace appbase {

  class options_dumper {

    using bpo = boost::program_options::options_description;
    using option_group_t = std::reference_wrapper<const bpo>;
    using entry_t = std::pair<const std::string, option_group_t>;

    public:
      struct value_info {
        bool multiple_allowed;
        bool composed;
        std::string value_type;
        fc::variant default_value;
      };

      struct option_entry {
        std::string name;
        std::string description;
        bool required;
        std::optional<value_info> value;
      };

      options_dumper();
      options_dumper(std::initializer_list<entry_t> entries);

      void add_options_group(const std::string& name, const bpo& group);
      void clear();
      std::string dump_to_string() const;

    private:
      std::unordered_map<std::string, option_group_t> _option_groups;
      
      static std::optional<value_info> get_value_info(const boost::program_options::value_semantic* semantic);
      static option_entry serialize_option(const boost::program_options::option_description& option);

  };

};
