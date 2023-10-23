#pragma once

namespace hive { namespace chain {

template <typename TFunction> struct fcall {};
template <typename TResult, typename... TArgs>
struct fcall<TResult(TArgs...)>
{
  using TNotification = std::function<TResult(TArgs...)>;

  fcall() = default;
  fcall(const TNotification& func, util::advanced_benchmark_dumper& dumper,
    const abstract_plugin& plugin, const std::string& context, const std::string& item_name)
    : _func(func), _benchmark_dumper(dumper), _context(context), _name(item_name) {}

  void operator () (TArgs&&... args)
  {
    if (_benchmark_dumper.is_enabled())
      _benchmark_dumper.begin();

    _func(std::forward<TArgs>(args)...);

    if (_benchmark_dumper.is_enabled())
      _benchmark_dumper.end( _context, _name );
  }

private:
  TNotification                    _func;
  util::advanced_benchmark_dumper& _benchmark_dumper;
  std::string                      _context;
  std::string                      _name;
};

template <typename TResult, typename... TArgs>
struct fcall<std::function<TResult(TArgs...)>>
  : public fcall<TResult(TArgs...)>
{
  typedef fcall<TResult(TArgs...)> TBase;
  using TBase::TBase;
};

template <bool IS_PRE_OPERATION, typename TSignal, typename TNotification>
boost::signals2::connection database::connect_impl( TSignal& signal, const TNotification& func,
  const abstract_plugin& plugin, int32_t group, const std::string& item_name )
{
  fcall<TNotification> fcall_wrapper( func, _benchmark_dumper, plugin,
    util::advanced_benchmark_dumper::generate_context_desc<IS_PRE_OPERATION>( plugin.get_name() ), item_name );

  return signal.connect(group, fcall_wrapper);
}

template< bool IS_PRE_OPERATION >
boost::signals2::connection database::any_apply_operation_handler_impl( const apply_operation_handler_t& func,
  const abstract_plugin& plugin, int32_t group )
{
  std::string context = util::advanced_benchmark_dumper::generate_context_desc< IS_PRE_OPERATION >( plugin.get_name() );
  auto complex_func = [this, func, &plugin, context]( const operation_notification& o )
  {
    std::string name;

    if (_benchmark_dumper.is_enabled())
    {
      name = o.op.get_stored_type_name();
      _benchmark_dumper.begin();
    }

    func( o );

    if (_benchmark_dumper.is_enabled())
      _benchmark_dumper.end( context, name );
  };

  if( IS_PRE_OPERATION )
    return _pre_apply_operation_signal.connect(group, complex_func);
  else
    return _post_apply_operation_signal.connect(group, complex_func);
}

}}
