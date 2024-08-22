#include <beekeeper_wasm/beekeeper_wasm_api.hpp>

#include <fc/log/logger_config.hpp>

#include <iostream>

#include <emscripten/bind.h>

using namespace beekeeper;
using namespace emscripten;

EMSCRIPTEN_BINDINGS(beekeeper_api_instance) {
  register_vector<std::string>("StringList");

  class_<beekeeper_api>("beekeeper_api")

    /*
      ****creation of an instance of beekeeper****
      PARAMS:
      --wallet-dir      : a directory where keys are stored in particular wallets. Default: `.`
      --unlock-timeout  : timeout for an unlocked wallet in seconds. After this time the wallet will be locked automatically when an attempt of execution of almost all(*) API endpoint occurs. Default: `900`
                         (*) except: `create_session`, `close_session`, `get_info`, `lock_all`, `set_timeout`
      --enable-logs     : whether logs can be generated. Default: `true`

      RESULT:
        an instance of a beekeeper
    */
    .constructor<std::vector<std::string>>()

    /*
      ****initialization of a beekeeper****
      PARAMS:
        nothing
      RESULT:
        {"status":true,"version":"a8329e82abe68bb222d8d92db0b09ec941c5f477"}
        status: if an initialization passed/failed
        version: a hash of current commit.
    */
    .function("init()", &beekeeper_api::init)

    /*
      ****creation of a session****
      PARAMS:
        salt: a salt used for creation of a token. Not required.
              If the salt is:
                - not given, chosen is a version (1)
                -     given, chosen is a version (2)
      RESULT:
        {"token":"440c44f01dde9ef65e7b88c6d44f3a929bbf0ff993c06efa6d942d40b08567f3"}
        token: a token of a session created explicitly. It can be used for further work for example: creating/closing wallets, importing keys, signing transactions etc.
    */
    .function("create_session()", select_overload<std::string()>(&beekeeper_api::create_session))                       //(1)
    .function("create_session(salt)", select_overload<std::string(const std::string&)>(&beekeeper_api::create_session)) //(2)

    /*
      ****closing of a session****
      PARAMS:
        token: a token representing a session
      RESULT:
        nothing
    */
    .function("close_session(token)", &beekeeper_api::close_session)

    /*
      ****creation of a wallet****
      PARAMS:
        token:        a token representing a session
        wallet_name:  a name of wallet
        password:     a password used for creation of a wallet. Not required and in this case a password is automatically generated.
                      If the password is:
                       - not given, chosen is a version (1)
                       -     given, chosen is a version (2)
      RESULT:
        {"password":"PW5KNCWdnMZFKzrvVyA2xwKLRxcAZWxPoyGVSN9r38te3p1ceEjo1"}
        password: a password of newly created a wallet
    */
    .function("create(token, wallet_name)", select_overload<std::string(const std::string&, const std::string&)>(&beekeeper_api::create))                     //(1)
    .function("create(token, wallet_name, password)", select_overload<std::string(const std::string&, const std::string&, const std::string&)>(&beekeeper_api::create)) //(2)

    /*
      ****unlocking of a wallet****
      PARAMS:
        token:        a token representing a session
        wallet_name:  a name of wallet
        password:     a wallet's password
      RESULT:
        nothing
    */
    .function("unlock(token, wallet_name, password)", &beekeeper_api::unlock)

    /*
      ****opening of a wallet****
      PARAMS:
        token:        a token representing a session
        wallet_name:  a name of wallet
      RESULT:
        nothing
    */
    .function("open(token, wallet_name)", &beekeeper_api::open)

    /*
      ****closing of a wallet****
      PARAMS:
        token:        a token representing a session
        wallet_name:  a name of wallet
      RESULT:
        nothing
    */
    .function("close(token, wallet_name)", &beekeeper_api::close)

    /*
      ****setting a timeout for a session****
      PARAMS:
        token:    a token representing a session
        seconds:  number of seconds
      RESULT:
        nothing
    */
    .function("set_timeout(token, seconds)", &beekeeper_api::set_timeout)

    /*
      ****locking all wallets****
      PARAMS:
        token: a token representing a session
      RESULT:
        nothing
    */
    .function("lock_all(token)", &beekeeper_api::lock_all)

    /*
      ****locking a wallet****
      PARAMS:
        token:        a token representing a session
        wallet_name:  a name of wallet
      RESULT:
        nothing
    */
    .function("lock(token, wallet_name)", &beekeeper_api::lock)

    /*
      ****importing of a private key into a wallet****
      PARAMS:
        token:        a token representing a session
        wallet_name:  a name of wallet
        wif_key:      a private key
      RESULT:
        {"public_key":"STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"}
        public_key: a public key corresponding to a private key
    */
    .function("import_key(token, wallet_name, wif_key)", &beekeeper_api::import_key)

    /*
      ****importing of private keys into a wallet****
      PARAMS:
        token:        a token representing a session
        wallet_name:  a name of wallet
        wif_key:      set of private keys
      RESULT:
        {"public_keys":["STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa", "STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh"]}
        public_keys: public keys corresponding to private keys
    */
    .function("import_keys(token, wallet_name, wif_keys)", &beekeeper_api::import_keys)

    /*
      ****removing of a private key from a wallet****
      PARAMS:
        token:        a token representing a session
        wallet_name:  a name of wallet
        public_key:   a public key corresponding to a private key that is stored in a wallet
      RESULT:
        nothing
    */
    .function("remove_key(token, wallet_name, public_key)", &beekeeper_api::remove_key)

    /*
      ****listing of all opened wallets****
      PARAMS:
        token: a token representing a session
      RESULT:
        {"wallets":[{"name":"wallet_a","unlocked":true},{"name":"wallet_b","unlocked":true}]}
        wallets: a set of all opened wallets. Every wallet has information:
          name: a name of wallet
          unlocked: information if a wallet is opened/closed
    */
    .function("list_wallets(token)", &beekeeper_api::list_wallets)

    /*
      ****listing of all public keys****
      PARAMS:
        token:        a token representing a session
        wallet_name:  a name of wallet where public keys are retrieved from
                      If the name of wallet is:
                       - not given, chosen is a version (1)
                       -     given, chosen is a version (2)
      RESULT:
        {"keys":[{"public_key":"6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"},{"public_key":"6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"}]}
        keys: a set of all keys for all unlocked wallets. Every key has information:
          public_key: a public key corresponding to a private key that is stored in a wallet
    */
    .function("get_public_keys(token)", select_overload<std::string(const std::string&)>(&beekeeper_api::get_public_keys))                                  //(1)
    .function("get_public_keys(token, wallet_name)", select_overload<std::string(const std::string&, const std::string&)>(&beekeeper_api::get_public_keys)) //(2)

    /*
      ****signing a transaction by signing a digest of the transaction****
      PARAMS:
        token:      a token representing a session
        public_key: a public key corresponding to a private key that is stored in a wallet. It will be used for creation of a signature
        sig_digest: a digest of a transaction
        wallet_name:  a name of wallet where public keys are searched
                      If the name of wallet is:
                       - not given, chosen is a version (1)
                       -     given, chosen is a version (2)
      RESULT:
        { "signature":"1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340"}
        signature: a signature of a transaction
    */
    .function("sign_digest(token, sig_digest, public_key)", select_overload<std::string(const std::string&, const std::string&, const std::string&)>(&beekeeper_api::sign_digest))              //(1)
    .function("sign_digest(token, sig_digest, public_key, wallet_name)", select_overload<std::string(const std::string&, const std::string&, const std::string&, const std::string&)>(&beekeeper_api::sign_digest)) //(2)

    /*
      ****information about a session****
      PARAMS:
        token:        a token representing a session
      RESULT:
        {"now":"2023-07-25T09:41:51","timeout_time":"2023-07-25T09:56:51"}
        now:          current server's time
        timeout_time: time when wallets will be automatically closed
    */
    .function("get_info(token)", &beekeeper_api::get_info)

    /*
      ****information about a version****
      RESULT:
        {"version":"d2abc7e9318fce5a10c73cda8beb57f46fe37247"}
        version: a version based on git hash
    */
    .function("get_version()", &beekeeper_api::get_version)

    /*
      ****testing if a private key corresponding to a public key exists in a wallet****
      PARAMS:
        token:        a token representing a session
        wallet_name:  a name of wallet
        public_key:   a public key corresponding to a private key that is stored in a wallet
      RESULT:
        {"exists":true}
        exists: true if a private key exists otherwise false
    */
    .function("has_matching_private_key(token, wallet_name, public_key)", &beekeeper_api::has_matching_private_key)

    /*
      ****Encrypting given content represented by a string****
      PARAMS:
        token:            a token representing a session
        from_public_key:  a creator's public key
        to_public_key:    a receiver's public key
        wallet_name:      a name of wallet
        content:          a string to encrypt

      WARNING: Current time is used as implicit nonce, so subsequent calls to this version can result in different results.

      RESULT:
        {"encrypted_content":"12796218200812abcd032de"}
        encrypted_content: an encrypted string
    */
    .function("encrypt_data(token, from_public_key, to_public_key, wallet_name, content)", select_overload<std::string(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&)>(&beekeeper_api::encrypt_data))

    /*
      ****Encrypting given content represented by a string****
      PARAMS:
        token:            a token representing a session
        from_public_key:  a creator's public key
        to_public_key:    a receiver's public key
        wallet_name:      a name of wallet
        content:          a string to encrypt
        nonce:            an explicit nonce to be used for encryption process
      RESULT:
        {"encrypted_content":"12796218200812abcd032de"}
        encrypted_content: an encrypted string
    */

    .function("encrypt_data(token, from_public_key, to_public_key, wallet_name, content, nonce)", select_overload<std::string(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&, unsigned int)>(&beekeeper_api::encrypt_data))

    /*
      ****Decrypting given content represented by a string. Private keys for a receiver and a creator must be accessible in unlocked wallets****
      PARAMS:
        token:              a token representing a session
        from_public_key:    a creator's public key
        to_public_key:      a receiver's public key
        wallet_name:        a name of wallet
        encrypted_content:  a string to encrypt
      RESULT:
        {"decrypted_content":"this is my content"}
        decrypted_content: an decrypted string
    */
    .function("decrypt_data(token, from_public_key, to_public_key, wallet_name, encrypted_content)", &beekeeper_api::decrypt_data)

    /*
      ****testing if a wallet exists****
      PARAMS:
        token:        a token representing a session
        wallet_name:  a name of wallet
      RESULT:
        {"exists":true}
        exists: true if a wallet exists otherwise false
    */
    .function("has_wallet(token, wallet_name)", &beekeeper_api::has_wallet)
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

  //"This function does nothing... You have to instantiate beekeeper_api on JS side to play with..."

  return 0;
}

