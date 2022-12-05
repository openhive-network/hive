#pragma once
#include <boost/program_options.hpp>
#include <fc/io/json.hpp>

#include <algorithm>
#include <initializer_list>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace appbase {

  class options_dumper {

    using bpo = boost::program_options::options_description;
    using option_group_t = std::reference_wrapper<const bpo>;
    using entry_t = std::pair<const std::string, option_group_t>;

    public:

      struct option_entry {
        std::string name;
        std::string description;
        bool required;
      };

      options_dumper() {}
      options_dumper(std::initializer_list<entry_t> entries) 
        : _option_groups(std::move(entries)) {}

      void add_options_group(const std::string& name, const bpo& group)
      {
        _option_groups.emplace(name, group);
      }

      void clear()
      {
        _option_groups.clear();
      }

      std::string dump_to_string() const
      {
        std::ostringstream ss;
        ss << '{';
        ss << '\n';

        bool comma = false;
        for (const auto& group : _option_groups)
        {
          if (comma)
            ss << ',' << '\n';

          std::vector<option_entry> entries;

          for (const auto& option : group.second.get().options()) {
            entries.emplace_back(serialize_option(*option));
          }

          ss << '"' << group.first << '"' << ": ";
          ss << fc::json::to_pretty_string(entries);

          comma = true;
        }

        ss << '}';

        return ss.str();
      }

    private:
      std::unordered_map<std::string, option_group_t> _option_groups;

      option_entry serialize_option(const boost::program_options::option_description& option) const
      {
        
        return {
          option.long_name(),
          option.description(),
          option.semantic()->is_required(),
        };
      }
  };

};

FC_REFLECT(appbase::options_dumper::option_entry, (name)(description)(required));
