#include <appbase/options_dumper.hpp>

#include <boost/any.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/variant_object.hpp>

#include <vector>


namespace appbase {

template<typename T>
fc::variant get_default_value(const boost::any& default_value )
{
  if( default_value.empty() )
    return fc::variant( std::string() );

  return boost::lexical_cast<std::string>( boost::any_cast<T>( default_value ) );
}

template<>
fc::variant get_default_value<bool>(const boost::any& default_value )
{
  if( default_value.empty() )
    return fc::variant( std::string() );

  std::ostringstream _ss;
  _ss << std::boolalpha << boost::any_cast<bool>( default_value );

  return _ss.str();
}

template<>
fc::variant get_default_value<std::vector<std::string>>(const boost::any& default_value )
{
  if( default_value.empty() )
    return fc::variant( std::vector<std::string>() );

  std::vector<std::string> _v;
  for (const auto& item : boost::any_cast<std::vector<std::string>>( default_value ) ) 
    _v.emplace_back( boost::lexical_cast<std::string>( item ) );

  return fc::variant( std::move( _v ) );
}

template<typename T>
void handle_type( options_dumper::value_info& val_info, const boost::program_options::typed_value_base* typed, const std::string& type_name )
{
  auto* _typed = dynamic_cast<const boost::program_options::typed_value<T>*>( typed );
  assert( _typed );

  fc::variant _def_value;
  boost::any _def_value_any;
  _typed->apply_default( _def_value_any );

  val_info.required       = _typed->is_required();
  val_info.multitoken     = _typed->max_tokens() > 1;
  val_info.composed       = _typed->is_composing();
  val_info.value_type     = type_name;
  val_info.default_value  = get_default_value<T>( _def_value_any );
}

void try_handle_type( options_dumper::value_info& val_info, const boost::program_options::typed_value_base* typed )
{
  if( typed->value_type() == typeid(bool) )
    handle_type<bool>( val_info, typed, "bool" );
  else if( typed->value_type() == typeid(int8_t) )
    handle_type<int8_t>( val_info, typed, "byte" );
  else if( typed->value_type() == typeid(int16_t) )
    handle_type<int16_t>( val_info, typed, "short" );
  else if( typed->value_type() == typeid(int32_t) )
    handle_type<int32_t>( val_info, typed, "int" );
  else if( typed->value_type() == typeid(int64_t) )
    handle_type<int64_t>( val_info, typed, "long" );
  else if( typed->value_type() == typeid(uint8_t) )
    handle_type<uint8_t>( val_info, typed, "ubyte" );
  else if( typed->value_type() == typeid(uint16_t) )
    handle_type<uint16_t>( val_info, typed, "ushort" );
  else if( typed->value_type() == typeid(uint32_t) )
    handle_type<uint32_t>( val_info, typed, "uint" );
  else if( typed->value_type() == typeid(uint64_t) )
    handle_type<uint64_t>( val_info, typed, "ulong" );
  else if( typed->value_type() == typeid(std::string) )
    handle_type<std::string>( val_info, typed, "string" );
  else if( typed->value_type() == typeid(fc::string) )
    handle_type<fc::string>( val_info, typed, "string" );
  else if( typed->value_type() == typeid(boost::filesystem::path) )
    handle_type<boost::filesystem::path>( val_info, typed, "path" );
  else if( typed->value_type() == typeid(std::vector<std::string>) )
    handle_type<std::vector<std::string>>( val_info, typed, "string_array" );
  else
    val_info.value_type = "unknown";
}

options_dumper::options_dumper(std::initializer_list<entry_t> entries) 
  : option_groups( std::move(entries) ) {}

std::optional<options_dumper::value_info> options_dumper::get_value_info( const boost::shared_ptr<const boost::program_options::value_semantic>& semantic ) const
{
  if( !semantic )
    return std::nullopt;

  auto* _typed = dynamic_cast<const boost::program_options::typed_value_base*>( semantic.get() );
  if( !_typed )
    return std::nullopt;

  value_info _info;
  try_handle_type( _info, _typed );

  return _info;
}

options_dumper::option_entry options_dumper::serialize_option( const boost::program_options::option_description& option ) const
{
  return
    {
      option.long_name(),
      option.description(),
      get_value_info( option.semantic() ),
    } ;
}

std::string options_dumper::dump_to_string() const
{
  fc::mutable_variant_object _obj;

  for( const auto& group : option_groups )
  {
    std::vector<option_entry> _entries;

    for( const auto& option : group.second.options() ) 
      _entries.emplace_back( serialize_option( *option ) );

    _obj[group.first] = std::move( _entries );
  }

  return fc::json::to_pretty_string( _obj );
}

};

FC_REFLECT(appbase::options_dumper::option_entry, (name)(description)(value));
FC_REFLECT(appbase::options_dumper::value_info, (required)(multitoken)(composed)(value_type)(default_value));
