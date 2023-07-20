#pragma once

#include <boost/thread/mutex.hpp>
#include <openssl/opensslconf.h>
#ifndef OPENSSL_THREADS
# error "OpenSSL must be configured to support threads"
#endif
#include <openssl/crypto.h>

namespace fc {

/* This stuff has to go somewhere, I guess this is as good a place as any...
  OpenSSL isn't thread-safe unless you give it access to some mutexes,
  so the CRYPTO_set_id_callback() function needs to be called before there's any
  chance of OpenSSL being accessed from multiple threads.
*/
struct openssl_thread_config
{
  static boost::mutex* openssl_mutexes;
  static unsigned long get_thread_id();
  static void locking_callback(int mode, int type, const char *file, int line);
  openssl_thread_config();
  ~openssl_thread_config();
};
openssl_thread_config openssl_thread_config_manager;

boost::mutex*         openssl_thread_config::openssl_mutexes = nullptr;

} // namespace fc
