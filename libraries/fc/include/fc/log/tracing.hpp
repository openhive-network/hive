#pragma once
#include <string>
#include <fc/network/http/connection.hpp>
#include <fc/variant.hpp>
#ifndef FC_DISABLE_TRACING
#include <memory>

/* normally, you won't need to know anything about the details of OpenTracing, but 
 * a few opentracing classes are exposed for cases where you need low-level 
 * access, like if you need to transport a context over a custom protocol
 */
namespace opentracing
{
  inline namespace v3_unstable
  {
    class SpanContext;
    class Span;
  }
}

namespace fc
{

class span 
{
  public:
    class impl;
  private:
    std::unique_ptr<impl> my;
  public:
    /** construct a new span.  If there is an active span in this task, this will automatically
     * become a child of that span.  If there is not, it will be a top-level span
     */
    span(const std::string& span_name, bool always_create_span);
    /** construct a new span.  If there is an active span in this task, this will automatically
     * become a child of that span.  If there is not, it will be a top-level span
     */
    span(const char* filename, int line_number, const std::string& span_name, bool always_create_span);
    /** construct a new span.  If there is an active span in this task, this will automatically
     * become a child of that span.  If there is not, it will be a top-level span
     */
    span(const char* filename, int line_number, const std::string& span_name_format_string, const fc::variant_object& substitutions, bool always_create_span);

    ~span();

    void set_tag(const std::string& key, const fc::variant& value);
};

class span_context {
  class impl;
  private:
    std::unique_ptr<impl> my;
    friend class span_parent_context_manager;
  public:
    span_context();
    span_context(std::unique_ptr<opentracing::SpanContext> context);
    span_context(span_context&& other);
    ~span_context();
    span_context& operator=(span_context&& other);
};

class span_parent_context_manager {
  class impl;
  private:
    std::unique_ptr<impl> my;
  public:
    span_parent_context_manager();
    span_parent_context_manager(std::unique_ptr<opentracing::SpanContext> parent_context);
    span_parent_context_manager(span_context&& parent_context);
    void set_context(std::unique_ptr<opentracing::SpanContext> parent_context);
    ~span_parent_context_manager();
};

bool span_is_active();
opentracing::Span* get_current_opentracing_span();
opentracing::SpanContext* get_task_parent_span_context();
std::unique_ptr<opentracing::SpanContext> set_task_parent_span_context(std::unique_ptr<opentracing::SpanContext> parent_context);
span_context clone_task_span_context();

void fc_tlog(const std::string& message);
void fc_tlog(opentracing::Span* span, const std::string& message);

void span_set_tag(const std::string& key, const fc::variant& value);

span_context extract_opentracing_context_from_http_headers(const std::vector<fc::http::header>& headers);
void inject_opentracing_context_into_http_headers(std::vector<fc::http::header>& headers);

namespace detail
{
  class dynamically_loaded_tracer_impl;
}

/** 
 * This class allows you to load a tracing plugin at runtime.  
 * Construct an instance of this class, passing the path of the shared library and 
 * the config file before you use any tracing features; the plugin will be unloaded
 * when this class goes out of scope, so you probably want to initialize this 
 * early (near the beginning of main()).
 *
 * I have only tested this using the Jaeger plugin from https://github.com/jaegertracing.
 * The plugin isn't built by default; configure it with JAEGERTRACING_PLUGIN defined 
 * to ON to build it.  You will probably need to make sure it builds against the same
 * version of OpenTracing as the one in FC's vendor/opentracing-cpp.
 */
class dynamically_loaded_tracer
{
  private:
    std::unique_ptr<detail::dynamically_loaded_tracer_impl> my;
  public:
    dynamically_loaded_tracer(const std::string& plugin_filename, const std::string& config_filename);
    ~dynamically_loaded_tracer();
};

#define FC_SPAN_GENERATE_PARAMETER_NAME(VALUE) BOOST_PP_LPAREN() BOOST_PP_STRINGIZE(VALUE), fc::variant(VALUE) BOOST_PP_RPAREN()
#define FC_SPAN_DONT_GENERATE_PARAMETER_NAME(NAME, VALUE) BOOST_PP_LPAREN() NAME, fc::variant(VALUE) BOOST_PP_RPAREN()
#define FC_SPAN_GENERATE_PARAMETER_NAMES_IF_NEEDED(r, data, PARAMETER_AND_MAYBE_NAME) BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE PARAMETER_AND_MAYBE_NAME,1),FC_SPAN_GENERATE_PARAMETER_NAME,FC_SPAN_DONT_GENERATE_PARAMETER_NAME)PARAMETER_AND_MAYBE_NAME

#define FC_SPAN_CONSTRUCTOR_ARGS_STRING_ONLY(FORMAT) \
   (__FILE__, __LINE__, FORMAT, false)
#define FC_SPAN_CONSTRUCTOR_ARGS_WITH_SUBSTITUTIONS(FORMAT, SUBSTITUTIONS) \
   (__FILE__, __LINE__, FORMAT, fc::mutable_variant_object() BOOST_PP_SEQ_FOR_EACH(FC_SPAN_GENERATE_PARAMETER_NAMES_IF_NEEDED, _, BOOST_PP_VARIADIC_SEQ_TO_SEQ(SUBSTITUTIONS)), false)
#define FC_SPAN_CONSTRUCTOR_ARGS_WITH_SUBSTITUTIONS_AND_FLAGS(FORMAT, SUBSTITUTIONS, FLAGS) \
   (__FILE__, __LINE__, FORMAT, fc::mutable_variant_object() BOOST_PP_SEQ_FOR_EACH(FC_SPAN_GENERATE_PARAMETER_NAMES_IF_NEEDED, _, BOOST_PP_VARIADIC_SEQ_TO_SEQ(SUBSTITUTIONS)), FLAGS)

#define FC_SPAN(...) \
   fc::span span_holder BOOST_PP_EXPAND(BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__),1),FC_SPAN_CONSTRUCTOR_ARGS_STRING_ONLY,BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__),2),FC_SPAN_CONSTRUCTOR_ARGS_WITH_SUBSTITUTIONS,FC_SPAN_CONSTRUCTOR_ARGS_WITH_SUBSTITUTIONS_AND_FLAGS))(__VA_ARGS__))

#define fc_span(...) fc::span BOOST_PP_EXPAND(BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__),1),FC_SPAN_CONSTRUCTOR_ARGS_STRING_ONLY,FC_SPAN_CONSTRUCTOR_ARGS_WITH_SUBSTITUTIONS)(__VA_ARGS__))

#define FC_TLOG_MESSAGE_STRING_ONLY(FORMAT) \
  fc::fc_tlog(FORMAT)
#define FC_TLOG_MESSAGE_WITH_SUBSTITUTIONS(FORMAT, ...) \
  fc::fc_tlog(fc::format_string(FORMAT, fc::limited_mutable_variant_object( FC_MAX_LOG_OBJECT_DEPTH, true ) BOOST_PP_SEQ_FOR_EACH(FC_LOG_MESSAGE_GENERATE_PARAMETER_NAMES_IF_NEEDED, _, BOOST_PP_VARIADIC_SEQ_TO_SEQ(__VA_ARGS__))))

#define tlog(...) \
  BOOST_PP_EXPAND(BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__),1),FC_TLOG_MESSAGE_STRING_ONLY,FC_TLOG_MESSAGE_WITH_SUBSTITUTIONS)(__VA_ARGS__))
} // end namespace fc
#else // FC_DISABLE_TRACING

namespace fc
{

class span 
{
  public:
    span(const std::string& span_name, bool always_create_span) {}
    span(const char* filename, int line_number, const std::string& span_name, bool always_create_span) {}
    span(const char* filename, int line_number, const std::string& span_name_format_string, const fc::variant_object& substitutions, bool always_create_span) {}
    void set_tag(const std::string& key, const fc::variant& value) {}
    ~span() {}
};

class span_context {
  public:
    span_context() {}
    span_context(std::unique_ptr<opentracing::SpanContext> context) {}
    span_context(span_context&& other) {}
};

class span_parent_context_manager {
  public:
    span_parent_context_manager() {}
    span_parent_context_manager(std::unique_ptr<opentracing::SpanContext> parent_context) {}
    span_parent_context_manager(span_context&& parent_context) {}
    void set_context(std::unique_ptr<opentracing::SpanContext> parent_context) {}
};

inline bool span_is_active() { return false; }
inline opentracing::Span* get_current_opentracing_span() { return nullptr; }
inline opentracing::SpanContext* get_task_parent_span_context() { return nullptr; }
//inline std::unique_ptr<opentracing::SpanContext> set_task_parent_span_context(std::unique_ptr<opentracing::SpanContext> parent_context) { return std::unique_ptr<opentracing::SpanContext>(); }
inline span_context clone_task_span_context() { return span_context(); }

inline void tlog(const std::string& message) {}
inline void tlog(opentracing::Span* span, const std::string& message) {}

inline void span_set_tag(const std::string& key, const fc::variant& value) {}

inline span_context extract_opentracing_context_from_http_headers(const std::vector<fc::http::header>& headers) { return span_context(); }
inline void inject_opentracing_context_into_http_headers(std::vector<fc::http::header>& headers) {}

class dynamically_loaded_tracer
{
  public:
    dynamically_loaded_tracer(const std::string& plugin_filename, const std::string& config_filename) {}
};

#define FC_SPAN(...) do {} while (0)
#define fc_span(...) fc::span("", false)

} // end namespace fc
#endif //FC_DISABLE_TRACING
