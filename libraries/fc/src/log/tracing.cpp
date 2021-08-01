#include <fstream>
#include <fc/thread/thread_specific.hpp>
#include "../thread/thread_d.hpp"
#include <fc/log/logger.hpp>

#include <fc/log/tracing.hpp>
#ifndef FC_DISABLE_TRACING
#include <opentracing/dynamic_load.h>
#include <opentracing/span.h>

#include <boost/algorithm/string.hpp>

namespace fc
{


fc::task_specific_ptr<span::impl> task_current_span;

class span::impl 
{
  public:
    impl* parent_span;
    std::unique_ptr<opentracing::Span> opentracing_span;

    impl(const std::string& span_name, 
         const char* filename,
         int line_number,
         const std::shared_ptr<opentracing::Tracer>& tracer,
         const opentracing::SpanContext* context) :
      parent_span(task_current_span.get())
    {
      if (context)
        opentracing_span = tracer->StartSpan(span_name, {opentracing::ChildOf(context)});
      else
        opentracing_span = tracer->StartSpan(span_name);
      if (filename && line_number)
        opentracing_span->SetTag("location", std::string(filename) + ":" + std::to_string(line_number));
      task_current_span.reset(this);
    }
};

namespace detail
{
  const opentracing::SpanContext* get_current_span_context()
  {
    // First, if there's an active span, use its context
    if (task_current_span)
      return &task_current_span->opentracing_span->context();
    return get_task_parent_span_context();
  }

  class variant_to_value_visitor : public fc::variant::visitor
  {
    public:
      mutable opentracing::Value value;
      /// handles null_type variants
      void handle() const override { value = opentracing::Value(); }
      void handle(const int64_t& v) const override { value = opentracing::Value(v); }
      void handle(const uint64_t& v) const override { value = opentracing::Value(v); }
      void handle(const double& v) const override { value = opentracing::Value(v); }
      void handle(const bool& v) const override { value = opentracing::Value(v); }
      void handle(const std::string& v) const override { value = opentracing::Value(v); }
      void handle(const variant_object& v) const override
      {
        opentracing::Dictionary dict;
        for (auto iter = v.begin(); iter != v.end(); ++iter)
        {
          variant_to_value_visitor visitor;
          iter->value().visit(visitor);
          dict.insert(std::make_pair(iter->key(), visitor.value));
        }
        value = opentracing::Value(dict); 
      }
      void handle(const variants& v) const override 
      {
        opentracing::Values values;
        for (const auto& val : v)
        {
          variant_to_value_visitor visitor;
          val.visit(visitor);
          values.push_back(std::move(visitor.value));
        }
        value = opentracing::Value(values); 
      }
  };
}


bool span_is_active()
{
  return (bool)task_current_span;
}

opentracing::Span* get_current_opentracing_span()
{
  return task_current_span ? task_current_span->opentracing_span.get() : nullptr;
}

opentracing::SpanContext* get_task_parent_span_context()
{
  context* current_context = thread::current().my->current;
  if (!current_context || !current_context->cur_task)
    return thread::current().my->non_task_parent_span_context.get();
  if (current_context->cur_task->_parent_span_context)
    return current_context->cur_task->_parent_span_context.get();
  return nullptr;
}

std::unique_ptr<opentracing::SpanContext> set_task_parent_span_context(std::unique_ptr<opentracing::SpanContext> parent_context)
{
  context* current_context = thread::current().my->current;
  std::unique_ptr<opentracing::SpanContext> old_context;
  if (!current_context || !current_context->cur_task)
  {
    old_context = std::move(thread::current().my->non_task_parent_span_context);
    thread::current().my->non_task_parent_span_context = std::move(parent_context);
  }
  else
  {
    old_context = std::move(current_context->cur_task->_parent_span_context);
    current_context->cur_task->_parent_span_context = std::move(parent_context);
  }
  return old_context;
}

span_context clone_task_span_context()
{
  if (task_current_span)
    return span_context(task_current_span->opentracing_span->context().Clone());
  else
    return span_context();
}

void fc_tlog(opentracing::Span* span, const std::string& message)
{
  if (span)
    span->Log({{"message", message}});
}

void fc_tlog(const std::string& message)
{
  opentracing::Span* span = get_current_opentracing_span();
  fc_tlog(span, message);
}

void span_set_tag(const std::string& key, const fc::variant& value)
{
  if (task_current_span)
  {
    detail::variant_to_value_visitor visitor;
    value.visit(visitor);
    task_current_span->opentracing_span->SetTag(key, std::move(visitor.value));
  }
}

namespace detail
{
  struct fc_http_headers_carrier_reader : opentracing::TextMapReader 
  {
    explicit fc_http_headers_carrier_reader(const std::vector<fc::http::header>& data_)
      : data{data_} {}

    using F = std::function<opentracing::expected<void>(opentracing::string_view, opentracing::string_view)>;

    opentracing::expected<void> ForeachKey(F f) const override {
      // Iterate through all key-value pairs, the tracer will use the relevant keys to extract a span context.
      for (auto& header : data) 
      {
        auto was_successful = f(boost::to_lower_copy(header.key), header.val);
        if (!was_successful) // If the callback returns and unexpected value, bail out of the loop.
          return was_successful;
      }

      // Indicate successful iteration.
      return {};
    }
    const std::vector<fc::http::header>& data;
  };
  struct fc_http_headers_carrier_writer : opentracing::TextMapWriter {
    explicit fc_http_headers_carrier_writer(std::vector<fc::http::header>& data_)
      : data{data_} 
    {}

    opentracing::expected<void> Set(opentracing::string_view key,
                                    opentracing::string_view value) const override {
      opentracing::expected<void> result;
      data.push_back(fc::http::header(key, value));
      return result;
    }
    std::vector<fc::http::header>& data;
  };

}

span_context extract_opentracing_context_from_http_headers(const std::vector<fc::http::header>& headers)
{
  detail::fc_http_headers_carrier_reader carrier{headers};
  auto span_context_maybe = opentracing::Tracer::Global()->Extract(carrier);
  if (span_context_maybe)
    return span_context(std::move(*span_context_maybe));
  else
    return span_context();
}

void inject_opentracing_context_into_http_headers(std::vector<fc::http::header>& headers)
{
  ilog("inject headers");
  opentracing::Span* current_span = get_current_opentracing_span();
  if (current_span)
  {
    ilog("inject headers current span");

    detail::fc_http_headers_carrier_writer carrier{headers};
    opentracing::Tracer::Global()->Inject(current_span->context(), carrier);
  }
}

span::span(const std::string& span_name, bool always_create_span)
{
  std::shared_ptr<opentracing::Tracer> tracer = opentracing::Tracer::Global();
  if (tracer)
  {
    const opentracing::SpanContext* context = detail::get_current_span_context();
    if (context || always_create_span)
      my.reset(new impl(span_name, nullptr, 0, tracer, context));
  }
}

span::span(const char* filename, int line_number, const std::string& span_name, bool always_create_span)
{
  std::shared_ptr<opentracing::Tracer> tracer = opentracing::Tracer::Global();
  if (tracer)
  {
    const opentracing::SpanContext* context = detail::get_current_span_context();
    if (context || always_create_span)
      my.reset(new impl(span_name, filename, line_number, tracer, context));
  }
}

span::span(const char* filename, int line_number, const std::string& span_name_format_string, const fc::variant_object& substitutions, bool always_create_span)
{
  std::shared_ptr<opentracing::Tracer> tracer = opentracing::Tracer::Global();
  if (tracer)
  {
    const opentracing::SpanContext* context = detail::get_current_span_context();
    if (context || always_create_span)
      my.reset(new impl(fc::format_string(span_name_format_string, substitutions), filename, line_number, tracer, context));
  }
}

span::~span()
{
  if (my)
    task_current_span.reset(my->parent_span);
  else
    task_current_span.reset();
}

class span_context::impl 
{
  public:
    std::unique_ptr<opentracing::SpanContext> context;
};

span_context::span_context() :
  my(std::make_unique<impl>())
{
}

span_context::span_context(std::unique_ptr<opentracing::SpanContext> context) :
  my(std::make_unique<impl>())
{
  my->context = std::move(context);
}

span_context::span_context(span_context&& other) :
  my(std::make_unique<impl>())
{
  my->context = std::move(other.my->context);
}

span_context::~span_context()
{
}

span_context& span_context::operator=(span_context&& other)
{
  my->context = std::move(other.my->context);
  return *this;
}

class span_parent_context_manager::impl
{
  public:
    std::unique_ptr<opentracing::SpanContext> _old_parent_context;
};

span_parent_context_manager::span_parent_context_manager() :
  my(std::make_unique<span_parent_context_manager::impl>())
{
}

span_parent_context_manager::span_parent_context_manager(std::unique_ptr<opentracing::SpanContext> parent_context) :
  my(std::make_unique<span_parent_context_manager::impl>())
{
  my->_old_parent_context = set_task_parent_span_context(std::move(parent_context));
}

span_parent_context_manager::span_parent_context_manager(span_context&& parent_context) :
  my(std::make_unique<span_parent_context_manager::impl>())
{
  my->_old_parent_context = set_task_parent_span_context(std::move(parent_context.my->context));
}

void span_parent_context_manager::set_context(std::unique_ptr<opentracing::SpanContext> parent_context)
{
  set_task_parent_span_context(std::move(my->_old_parent_context));
  my->_old_parent_context = set_task_parent_span_context(std::move(parent_context));
}

span_parent_context_manager::~span_parent_context_manager()
{
  set_task_parent_span_context(std::move(my->_old_parent_context));
}



void span::set_tag(const std::string& key, const fc::variant& value)
{
  detail::variant_to_value_visitor visitor;
  value.visit(visitor);
  my->opentracing_span->SetTag(key, std::move(visitor.value));
}

namespace detail
{
  class dynamically_loaded_tracer_impl 
  {
    private:
      // when this goes out of scope, it will unload the library.  that needs to be after we close the tracer
      opentracing::expected<opentracing::DynamicTracingLibraryHandle> tracing_library_handle;
    public:
      dynamically_loaded_tracer_impl(const std::string& plugin_filename, const std::string& config_filename)
      {
        std::string error_message; 

        tracing_library_handle = opentracing::DynamicallyLoadTracingLibrary(plugin_filename.c_str(), error_message);
        if (tracing_library_handle)
        {
          // Read in the tracer's configuration.
          std::ifstream config_stream(config_filename);
          if (config_stream.good())
          {
            std::string tracer_config = std::string(std::istreambuf_iterator<char>(config_stream),
                                                    std::istreambuf_iterator<char>());

            // Construct a tracer
            const opentracing::TracerFactory& tracer_factory = tracing_library_handle->tracer_factory();
            opentracing::expected<std::shared_ptr<opentracing::Tracer>> tracer_maybe = tracer_factory.MakeTracer(tracer_config.c_str(), error_message);
            if (!tracer_maybe)
              elog("Failed to create tracer: ${error_message}", (error_message));
            else
              opentracing::Tracer::InitGlobal(*tracer_maybe);
          }
          else
            elog("Failed to open tracer config file ${config_filename}: ${error}", (config_filename)("error", std::strerror(errno)));
        }
        else
          elog("Failed to load tracer library ${error_message}", (error_message));
      }
      ~dynamically_loaded_tracer_impl() 
      {
        // clear the global singleton, this returns the previous value of the singleton
        auto tracer = opentracing::Tracer::InitGlobal(nullptr);
        // if there was a global singleton, close it here
        if (tracer)
          tracer->Close();
      }
  };
}

dynamically_loaded_tracer::dynamically_loaded_tracer(const std::string& plugin_filename, const std::string& config_filename) :
  my(new detail::dynamically_loaded_tracer_impl(plugin_filename, config_filename))
{}
dynamically_loaded_tracer::~dynamically_loaded_tracer()
{}

} // end namespace fc
#endif // FC_DISABLE_TRACING

