#include <beekeeper_wasm_app_api/beekeeper_wasm_api.hpp>

#include <emscripten/bind.h>

using namespace beekeeper;
using namespace emscripten;

EMSCRIPTEN_BINDINGS(beekeeper_api_instance) {
  register_vector<std::string>("StringList");

  class_<beekeeper_api>("beekeeper_api")
    .constructor<std::vector<std::string>>()
    .function("init", &beekeeper_api::init)
    .function("create_session", &beekeeper_api::create_session)
    .function("create", &beekeeper_api::create)
    .function("import_key", &beekeeper_api::import_key)
    ;
}
