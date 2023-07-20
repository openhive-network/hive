#include <fc/thread/thread.hpp>
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

unsigned long openssl_thread_config::get_thread_id()
{
#ifdef _WIN32
  return (unsigned long)::GetCurrentThreadId();
#else
  return (unsigned long)(&fc::thread::current()); //   TODO: should expose boost thread id
#endif
}

void openssl_thread_config::locking_callback(int mode, int type, const char *file, int line)
{
  if (mode & CRYPTO_LOCK)
    openssl_mutexes[type].lock();
  else
    openssl_mutexes[type].unlock();
}

// Warning: Things get complicated if third-party libraries also try to install their their own 
// OpenSSL thread functions.  Right now, we don't install our own handlers if another library has
// installed them before us which is a partial solution, but you'd really need to evaluate
// each library that does this to make sure they will play nice.
openssl_thread_config::openssl_thread_config()
{
  if (CRYPTO_get_id_callback() == NULL &&
      CRYPTO_get_locking_callback() == NULL)
  {
    openssl_mutexes = new boost::mutex[CRYPTO_num_locks()];
    CRYPTO_set_id_callback(&get_thread_id);
    CRYPTO_set_locking_callback(&locking_callback);
  }
}
openssl_thread_config::~openssl_thread_config()
{
  if ( CRYPTO_get_id_callback() != NULL &&
       CRYPTO_get_id_callback() == &get_thread_id)
  {
    CRYPTO_set_id_callback(NULL);
    CRYPTO_set_locking_callback(NULL);
    delete[] openssl_mutexes;
    openssl_mutexes = nullptr;
  }
}

} // namespace fc
