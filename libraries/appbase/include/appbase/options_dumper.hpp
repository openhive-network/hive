#pragma once
#include <boost/program_options.hpp>
#include <fc/variant.hpp>

#include <initializer_list>

namespace appbase {

  class options_dumper
  {

    public:

      using bpo     = boost::program_options::options_description;
      using entry_t = std::pair<const std::string, bpo>;
      using options_group_t = std::map<std::string, bpo>;

    private:

      options_group_t option_groups;

    public:

      options_dumper( std::initializer_list<entry_t> entries );

      std::string dump_to_string() const;
  };

};
