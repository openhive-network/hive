#include <fc/thread/spin_yield_lock.hpp>
#include <fc/time.hpp>
#include <boost/atomic.hpp>
#include <boost/memory_order.hpp>
#include <new>

#include <fc/valgrind.hpp>

namespace fc {
  void yield();

  #define define_self  boost::atomic<int>* self = (boost::atomic<int>*)&_lock

  spin_yield_lock::spin_yield_lock() 
  {
     HG_ANNOTATE_RWLOCK_CREATE(this);
     define_self;
     new (self) boost::atomic<int>();
     static_assert( sizeof(boost::atomic<int>) == sizeof(_lock), "" );
     self->store(unlocked);
  }

  spin_yield_lock::~spin_yield_lock()
  {
     HG_ANNOTATE_RWLOCK_DESTROY(this);
  }

  bool spin_yield_lock::try_lock() {
    define_self;
    const bool acquired = self->exchange(locked, boost::memory_order_acquire)!=locked;
    if (acquired) HG_ANNOTATE_RWLOCK_ACQUIRED(this, 1);
    return acquired;
  }

  bool spin_yield_lock::try_lock_for( const fc::microseconds& us ) {
    return try_lock_until( fc::time_point::now() + us );
  }

  bool spin_yield_lock::try_lock_until( const fc::time_point& abs_time ) {
     while( abs_time > time_point::now() ) {
        if( try_lock() ) 
           return true;
        yield(); 
     }
     return false;
  }

  void spin_yield_lock::lock() {
    define_self;
    while( self->exchange(locked, boost::memory_order_acquire)==locked) {
      yield(); 
    }
    HG_ANNOTATE_RWLOCK_ACQUIRED(this, 1);
  }

  void spin_yield_lock::unlock() {
    define_self;
    self->store(unlocked, boost::memory_order_release);
    HG_ANNOTATE_RWLOCK_RELEASED(this, 1);
  }
  #undef define_self

} // namespace fc
