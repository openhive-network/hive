#include <beekeeper_wasm/beekeeper_wasm_api.hpp>

#include <iostream>

#include <emscripten/bind.h>

using namespace beekeeper;
using namespace emscripten;

EMSCRIPTEN_BINDINGS(beekeeper_api_instance) {
  register_vector<std::string>("StringList");

  class_<beekeeper_api>("beekeeper_api")

    /*
      ****creation of an instance of beekeeper****
      params:
      --wallet-dir DIRECTORY  (directory where keys are stored)
      --salt       SALT       (a salt chosen by an user used to an encryption. Not required.)

      result:
        returns an instance of beekeeper
    */
    .constructor<std::vector<std::string>>()

    /*
      ****initialization of a beekeeper****
      params:
        nothing
      result:
        {"status":true,"token":"e653fc43f2c98df497ae7adf9874d7cf04b2dc90d551088aca56d18b87d8f0ba"}
        status: if an initialization passed/failed
        token:  A token of a session created implicitly. It can be used for further work for example: creating/closing wallets, importing keys, signing transactions etc.
    */
    .function("init", &beekeeper_api::init)

    /*
      ****creation of a session****
      params:
        salt: a salt used for creation of a token
      result:
        {"token":"440c44f01dde9ef65e7b88c6d44f3a929bbf0ff993c06efa6d942d40b08567f3"}
        token: A token of a session created explicitly. It can be used for further work for example: creating/closing wallets, importing keys, signing transactions etc.
    */
    .function("create_session", &beekeeper_api::create_session)

    /*
      ****closing of a session****
      params:
        token: a token representing a session
      result:
        nothing
    */
    .function("close_session", &beekeeper_api::close_session)

    /*
      ****creation of a wallet****
      params:
        token:        a token representing a session
        wallet_name:  a name of wallet
        password:     a password used for creation of a wallet. Not required and in this case a password is generated automatically.
      result:
        {"password":"PW5KNCWdnMZFKzrvVyA2xwKLRxcAZWxPoyGVSN9r38te3p1ceEjo1"}
        password: a password of newly created a wallet
    */
    .function("create", &beekeeper_api::create)

    /*
      ****unlocking of a wallet****
      params:
        token:        a token representing a session
        wallet_name:  a name of wallet
        password:     a wallet's password
      result:
        nothing
    */
    .function("unlock", &beekeeper_api::unlock)

    /*
      ****opening of a wallet****
      params:
        token:        a token representing a session
        wallet_name:  a name of wallet
      result:
        nothing
    */
    .function("open", &beekeeper_api::open)

    /*
      ****closing of a wallet****
      params:
        token:        a token representing a session
        wallet_name:  a name of wallet
      result:
        nothing
    */
    .function("close", &beekeeper_api::close)

    /*
      ****setting a timeout for a session****
      params:
        token:    a token representing a session
        seconds:  number of seconds
      result:
        nothing
    */
    .function("set_timeout", &beekeeper_api::set_timeout)

    /*
      ****locking all wallets****
      params:
        token: a token representing a session
      result:
        nothing
    */
    .function("lock_all", &beekeeper_api::lock_all)

    /*
      ****locking a wallet****
      params:
        token:        a token representing a session
        wallet_name:  a name of wallet
      result:
        nothing
    */
    .function("lock", &beekeeper_api::lock)

    /*
      ****importing of a private key into a wallet****
      params:
        token:        a token representing a session
        wallet_name:  a name of wallet
        wif_key:      a private key
      result:
        {"public_key":"6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"}
        public_key: a public key corresponding to a private key
    */
    .function("import_key", &beekeeper_api::import_key)

    /*
      ****removing of a private key from a wallet****
      params:
        token:        a token representing a session
        wallet_name:  a name of wallet
        password:     a wallet's password
        public_key:   a public key corresponding to a private key that is stored in a wallet
      result:
        nothing
    */
    .function("remove_key", &beekeeper_api::remove_key)

    /*
      ****listing of all opened wallets****
      params:
        token: a token representing a session
      result:
        {"wallets":[{"name":"wallet_a","unlocked":true},{"name":"wallet_b","unlocked":true}]}
        wallets: a set of all opened wallets. Every wallet has information:
          name: name of wallet
          unlocked: information if a wallet is opened/closed
    */
    .function("list_wallets", &beekeeper_api::list_wallets)

    /*
      ****listing of all public keys****
      params:
        token:  a token representing a session
      result:
        {"keys":[{"public_key":"6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"},{"public_key":"6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"}]}
        keys: a set of all keys for all unlocked wallets. Every key has information:
          public_key: a public key corresponding to a private key that is stored in a wallet
    */
    .function("get_public_keys", &beekeeper_api::get_public_keys)

    /*
      ****signing a transaction by signing sig_digest****
      params:
        token:      a token representing a session
        public_key: a public key corresponding to a private key that is stored in a wallet. It will be used for creation of a signature
        sig_digest: a digest of a transaction
      result:
        { "signature":"1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340"}
        signature: a signature of a transaction
    */
    .function("sign_digest", &beekeeper_api::sign_digest)

    /*
      ****signing a transaction presented in a binary form (hex)****
      params:
        token:        a token representing a session
        transaction:  a transaction in binary form (hex)
        chain_id:     id of a blockchain
        public_key:   a public key corresponding to a private key that is stored in a wallet. It will be used for creation of a signature
      result:
        { "signature":"1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340"}
        signature: a signature of a transaction
    */
    .function("sign_binary_transaction", &beekeeper_api::sign_binary_transaction)

    /*
      ****signing a transaction presented in a JSON form****
      params:
        token:        a token representing a session
        transaction:  a transaction in JSON form
        chain_id:     id of a blockchain
        public_key:   a public key corresponding to a private key that is stored in a wallet. It will be used for creation of a signature
      result:
        { "signature":"1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340"}
        signature: a signature of a transaction
    */
    .function("sign_transaction", &beekeeper_api::sign_transaction)

    /*
      ****information about a session****
      params:
        token:        a token representing a session
      result:
        {"now":"2023-07-25T09:41:51","timeout_time":"2023-07-25T09:56:51"}
        now:          current server's time
        timeout_time: time when wallets will be automatically closed
    */
    .function("get_info", &beekeeper_api::get_info)
    ;
}

/*
  Explanation lock/unlock/open/close

  A procedure of interaction with beekeeper would look like this:

    - open (open wallet file / mount hardware key)
    - unlock (pass password so private keys are available for signing)
    ... some operations ...
    - lock (wallet still can be unlocked again and is visible, but private keys are secure)
    - close (wallet file has been closed / hardware key has been unmounted, other app can access it now)

  close can be called without lock, which is done implicitly, same as unlock an be called without open
*/

int main() {
  // Main should not be run during TypeScript generation.
  //abort();
  std::cout << "This function does nothing... You have to instantiate beekeeper_api on JS side to play with..." << std::endl;
  return 0;
}

