#include <core/beekeeper_wasm_api.hpp>

#include <iostream>

#include <emscripten/bind.h>

using namespace beekeeper;
using namespace emscripten;

EMSCRIPTEN_BINDINGS(beekeeper_api_instance) {
  register_vector<std::string>("StringList");

  class_<beekeeper_api>("beekeeper_api")
    .constructor<std::vector<std::string>>()

    .function("init", &beekeeper_api::init)
   
    .function("create_session", &beekeeper_api::create_session)
    .function("close_session", &beekeeper_api::close_session)

    .function("create", &beekeeper_api::create)
    .function("unlock", &beekeeper_api::unlock)
    .function("open", &beekeeper_api::open)
    .function("close", &beekeeper_api::close)

    .function("set_timeout", &beekeeper_api::set_timeout)

    .function("lock_all", &beekeeper_api::lock_all)
    .function("lock", &beekeeper_api::lock)

    .function("import_key", &beekeeper_api::import_key)
    .function("remove_key", &beekeeper_api::remove_key)

    .function("list_wallets", &beekeeper_api::list_wallets)
    .function("get_public_keys", &beekeeper_api::get_public_keys)

    .function("sign_digest", &beekeeper_api::sign_digest)
    .function("sign_binary_transaction", &beekeeper_api::sign_binary_transaction)
    .function("sign_transaction", &beekeeper_api::sign_transaction)

    .function("get_info", &beekeeper_api::get_info)
    ;
}

int main() {
  // Main should not be run during TypeScript generation.
  //abort();
  std::cout << "This function does nothing... You have to instantiate beekeeper_api on JS side to play with..." << std::endl;
  return 0;
}

