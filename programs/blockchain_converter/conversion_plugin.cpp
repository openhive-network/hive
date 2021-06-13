#include <hive/utilities/key_conversion.hpp>

#include <iostream>

#include "conversion_plugin.hpp"

namespace hive { namespace converter { namespace plugins {

  void conversion_plugin_impl::print_wifs()const
  {
    std::cout << "Second authority wif private keys:\n"
      << "Owner:   " << key_to_wif( converter.get_second_authority_key( authority::owner ) ) << '\n'
      << "Active:  " << key_to_wif( converter.get_second_authority_key( authority::active ) ) << '\n'
      << "Posting: " << key_to_wif( converter.get_second_authority_key( authority::posting ) ) << '\n';
  }

} } } // hive::converter::plugins
