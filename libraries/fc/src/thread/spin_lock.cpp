#include <fc/thread/spin_lock.hpp>
#include <fc/time.hpp>
#include <boost/atomic.hpp>
#include <boost/memory_order.hpp>
#include <new>

#include <valgrind/helgrind.h>

namespace fc {
  #define define_self  boost::atomic<int>* self = (boost::atomic<int>*)&_lock
  spin_lock::spin_lock() 
  {
     ANNOTATE_RWLOCK_CREATE(this);
     define_self;
     new (self) boost::atomic<int>();
     static_assert( sizeof(boost::atomic<int>) == sizeof(_lock), "" );
     self->store(unlocked);
  }

  spin_lock::~spin_lock() 
  {
     ANNOTATE_RWLOCK_DESTROY(this);
  }

  bool spin_lock::try_lock() { 
    define_self;
    const bool acquired = self->exchange(locked, boost::memory_order_acquire)!=locked;
    if (acquired) ANNOTATE_RWLOCK_ACQUIRED(this, 1);
    return acquired;
  }

  bool spin_lock::try_lock_for( const fc::microseconds& us ) {
    return try_lock_until( fc::time_point::now() + us );
  }

  bool spin_lock::try_lock_until( const fc::time_point& abs_time ) {
     while( abs_time > time_point::now() ) {
        if( try_lock() ) 
           return true;
     }
     return false;
  }

  void spin_lock::lock() {
    define_self;
    while( self->exchange(locked, boost::memory_order_acquire)==locked) { }
    ANNOTATE_RWLOCK_ACQUIRED(this, 1);
  }

  void spin_lock::unlock() {
     define_self;
     self->store(unlocked, boost::memory_order_release);
    ANNOTATE_RWLOCK_RELEASED(this, 1);
  }

  #undef define_self

} // namespace fc
