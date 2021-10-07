#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include <boost/any.hpp>
#include <fc/static_variant.hpp>

namespace fc { namespace http {

  enum class version : unsigned
  {
    http_0_9 = 0, // Note: is obsolete for over 20 years and it will be most likely never used in this or any other hive related project
    http_1_0 = 1, // Provided header fields including rich metadata about both request and response
    http_1_1 = 2, // enables connection reuse
    http_2_0 = 3, // HPACK, multiple streams of data at once
    http_3_0 = 4, // QUIC
    http_unsupported = static_cast<unsigned>(-1)
  };

  namespace detail {
    // TODO: implement class processor_0_9;
    // TODO: implement class processor_1_0;
       class processor_1_1; // See processors/http_1_1.hpp
    // TODO: implement class processor_2_0;
    // TODO: implement class processor_3_0;
       class processor_default; // for sending http_version_not_supported response // See processors/http_unsupported.hpp

    static constexpr const char* default_http_version  = "HTTP/1.1";
    static constexpr const char* default_http_target   = "/";
    static constexpr const char* default_http_response = "200 OK";
  } // detail

  enum class request_method : unsigned
  {
    GET     = 0,
    POST    = 1,
    HEAD    = 2,
    PUT     = 3,
    DELETE  = 4,
    CONNECT = 5,
    OPTIONS = 6,
    TRACE   = 7,
    PATCH   = 8
  };

  enum class target_type : unsigned
  {
    path      = 0, // e.g. /a/b/c?d=e&f=g - used with GET, POST, HEAD, PUT, OPTIONS
    url       = 1, // e.g. https://www.hive.blog/ - used mostly with GET when connected to a proxy
    authority = 2, // e.g. hive.blog:80 - (with port) used with CONNECT when setting up an HTTP tunnel
    asterisk  = 3  // e.g. * - used with OPTIONS - representing the server as a whole
  };

  class http_target
  {
  private:
    std::string str_target;
    target_type type;

  public:
    http_target( const std::string& str_target = detail::default_http_target );

    /// Returns the string representation passed into the request target constructor
    const std::string& str()const;
    /// Returns the target type
    target_type        get()const;
  };

  class http_version
  {
  private:
    version _version;

  public:
    http_version( const std::string& str_version = detail::default_http_version );

    /// Returns the string representation passed into the request target constructor
    std::string str()const;
    /// Returns the request version
    version     get()const;
  };

  typedef std::unordered_map< std::string, std::string > headers_type;

  struct request
  {
    request_method method = request_method::GET; // defaults to GET
    http_target    target; // default to /
    http_version   version; // defaults to HTTP/1.1
    headers_type   headers; // defaults to (no headers)
    /// Cannot be empty in TRACE, GET, HEAD, DELETE, CONNECT and OPTIONS methods
    std::string    body; // defaults to empty body

    std::string to_string()const;
    void        from_string( const std::string& str );
  };

  enum class http_status_code : unsigned
  {
    // 1XX (informational) - The request was received, continuing process
    code_continue      = 100,
    switching_protocol = 101,
    processing         = 102, // (WebDAV)
    early_hints        = 103,

    // 2XX (successful) - The request was successfully received, understood, and accepted
    ok                            = 200,
    created                       = 201,
    accepted                      = 202,
    non_authoritative_information = 203,
    no_content                    = 204,
    reset_content                 = 205,
    partial_content               = 206,
    multi_status                  = 207, // (WebDAV)
    already_reported              = 208, // (WebDAV)
    im_used                       = 226, // HTTP Delta encoding

    // 3XX (redirection) - Further action needs to be taken in order to complete the request
    multiple_choice    = 300,
    moved_permamently  = 301,
    found              = 302,
    see_other          = 303,
    not_modified       = 304,
    use_proxy          = 305,
    unused             = 306,
    temporary_redirect = 307,
    permament_redirect = 308,


    // 4XX (client error) - The request contains bad syntax or cannot be fulfilled
    bad_request                     = 400,
    unauthorized                    = 401,
    payment_required                = 402,
    forbidden                       = 403,
    not_found                       = 404,
    method_not_allowed              = 405,
    not_acceptable                  = 406,
    proxy_authentication_required   = 407,
    request_timeout                 = 408,
    conflict                        = 409,
    gone                            = 410,
    length_required                 = 411,
    precondition_failed             = 412,
    payload_too_large               = 413,
    uri_too_long                    = 414,
    unsupported_media_type          = 415,
    range_not_satisfiable           = 416,
    expectation_failed              = 417,
    i_am_a_teapot                   = 418,
    misdirected_request             = 419,
    unprocessable_entity            = 422, // (WebDAV)
    locked                          = 423, // (WebDAV)
    failed_dependency               = 424, // (WebDAV)
    too_early                       = 425,
    upgrade_required                = 426,
    precondition_required           = 428,
    too_many_requests               = 429,
    request_header_fields_too_large = 431,
    unavailable_for_legal_reasons   = 451,

    // 5XX (server error) - The server failed to fulfill an apparently valid request
    internal_server_error           = 500,
    not_implemented                 = 501,
    bad_gateway                     = 502,
    service_unavailable             = 503,
    gateway_timeout                 = 504,
    http_version_not_supported      = 505,
    variant_also_negotiates         = 506,
    insufficient_storage            = 507, // (WebDAV)
    loop_detected                   = 508, // (WebDAV)
    not_extended                    = 510,
    network_authentication_required = 511
  };

  class http_status
  {
  private:
    http_status_code code;

  public:
    /// Parses "<http_code> <http_code_str>" into the status representation
    http_status( const std::string& status_str = detail::default_http_response );

    static std::string code_to_status_text( http_status_code code );

    /// Returns the string representation passed into the constructor
    std::string      str()const;
    /// Returns the status code (as unsigned)
    http_status_code get()const;
  };

  struct response
  {
    http_version   version; // defaults to HTTP/1.1
    http_status    status; // defaults to 200 OK
    headers_type   headers; // defaults to (no headers)
    std::string    body; // defaults to empty body

    std::string to_string()const;
    void        from_string( const std::string& str );
  };

  class processor;
  typedef std::shared_ptr< processor > processor_ptr;

  class processor
  {
  public:
    processor();
    virtual ~processor();

    /// Version of the HTTP that processor accepts
    virtual version get_version()const = 0;

    virtual std::string parse_request( const request& r )const     = 0;
    virtual request     parse_request( const std::string& r )const = 0;

    virtual std::string parse_response( const response& r )const    = 0;
    virtual response    parse_response( const std::string& r )const = 0;

    /// Returns processor for specific http version
    static processor_ptr get_for_version( version _http_v );
  };

} } // fc::http

FC_REFLECT( fc::http::request, (method)(target)(version)(headers)(body) );
FC_REFLECT( fc::http::response, (version)(status)(headers)(body) );
