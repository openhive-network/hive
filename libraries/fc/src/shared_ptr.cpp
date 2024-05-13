#include <fc/shared_ptr.hpp>
#include <boost/atomic.hpp>
#include <boost/memory_order.hpp>
#include <assert.h>

#include <valgrind/helgrind.h>

namespace fc {
  retainable::retainable()
  :_ref_count(1) { 
     static_assert( sizeof(_ref_count) == sizeof(boost::atomic<int32_t>), "failed to reserve enough space" );
  }

  retainable::~retainable() { 
    assert( _ref_count <= 0 );
    assert( _ref_count == 0 );
  }
  void retainable::retain() {
    ((boost::atomic<int32_t>*)&_ref_count)->fetch_add(1, boost::memory_order_relaxed );
  }

  void retainable::release() {
    boost::atomic_thread_fence(boost::memory_order_acquire);
    ANNOTATE_HAPPENS_BEFORE((long)this);
    if( 1 == ((boost::atomic<int32_t>*)&_ref_count)->fetch_sub(1, boost::memory_order_release ) ) {
      ANNOTATE_HAPPENS_AFTER((long)this);
      delete this;
    }
  }

  int32_t retainable::retain_count()const {
    return _ref_count;
  }
}
