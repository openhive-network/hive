#pragma once

#include <atomic>
#include <memory>
#include <mutex>

#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
#  define HIVE_HAS_STD_ATOMIC_SHARED_PTR 1
#endif

namespace hive { namespace chain {

/**
 * Portable replacement for std::atomic<std::shared_ptr<T>>.
 * libstdc++ provides the C++20 atomic specialization, but libc++ (macOS) does
 * not, so on that platform we fall back to a mutex-guarded shared_ptr.
 * The observable interface is limited to what the block log uses:
 * load(), store() and exchange(). Copying the wrapper itself (used when the
 * wrapper class lives inside a deque) performs an atomic load/store.
 */
template< typename T >
class atomic_shared_ptr
{
  public:
    atomic_shared_ptr() noexcept = default;

    atomic_shared_ptr( std::shared_ptr<T> p ) noexcept
    {
#if defined(HIVE_HAS_STD_ATOMIC_SHARED_PTR)
      _atomic.store( std::move( p ) );
#else
      std::lock_guard<std::mutex> lock( _mutex );
      _ptr = std::move( p );
#endif
    }

    atomic_shared_ptr( const atomic_shared_ptr& o ) noexcept
    {
      store( o.load() );
    }

    atomic_shared_ptr& operator=( const atomic_shared_ptr& o ) noexcept
    {
      if( this != &o )
        store( o.load() );
      return *this;
    }

    std::shared_ptr<T> load() const noexcept
    {
#if defined(HIVE_HAS_STD_ATOMIC_SHARED_PTR)
      return _atomic.load();
#else
      std::lock_guard<std::mutex> lock( _mutex );
      return _ptr;
#endif
    }

    void store( std::shared_ptr<T> p ) noexcept
    {
#if defined(HIVE_HAS_STD_ATOMIC_SHARED_PTR)
      _atomic.store( std::move( p ) );
#else
      std::lock_guard<std::mutex> lock( _mutex );
      _ptr = std::move( p );
#endif
    }

    std::shared_ptr<T> exchange( std::shared_ptr<T> p ) noexcept
    {
#if defined(HIVE_HAS_STD_ATOMIC_SHARED_PTR)
      return _atomic.exchange( std::move( p ) );
#else
      std::lock_guard<std::mutex> lock( _mutex );
      std::shared_ptr<T> old = std::move( _ptr );
      _ptr = std::move( p );
      return old;
#endif
    }

  private:
#if defined(HIVE_HAS_STD_ATOMIC_SHARED_PTR)
    std::atomic<std::shared_ptr<T>> _atomic;
#else
    mutable std::mutex   _mutex;
    std::shared_ptr<T>   _ptr;
#endif
};

} } // hive::chain
