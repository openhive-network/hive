#pragma once

#include <boost/program_options.hpp>

namespace hive { namespace utilities {

/**
 * @brief Wrapper class that unifies increased width of option descriptions
 *        across application.
 * The result is noticeable when options are printed in terminal.
 * The printing to config file is not affected.
 */
class options_description_ex : public boost::program_options::options_description
{
  public:
  
  options_description_ex() : 
    options_description( common_line_length, common_min_description_length )
  {}

  options_description_ex( const std::string& caption ) :
    options_description( caption, common_line_length, common_min_description_length )
  {}

  public:

  /// Default boost value is 80
  static constexpr unsigned common_line_length = 120;
  /// Default boost value is 40
  static constexpr unsigned common_min_description_length = 110;
};

} } // hive::utilities
