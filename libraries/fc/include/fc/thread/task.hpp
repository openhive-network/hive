#pragma once
#include <fc/thread/future.hpp>
#include <fc/thread/priority.hpp>
#include <fc/fwd.hpp>

namespace fc {
  struct context;
  class spin_lock;

   namespace detail
   {
      struct specific_data_info
      {
         void* value;
         void (*cleanup)(void*);
         specific_data_info() :
            value(0),
            cleanup(0)
            {}
         specific_data_info(void* value, void (*cleanup)(void*)) :
            value(value),
            cleanup(cleanup)
         {}
      };
      void* get_task_specific_data(unsigned slot);
      void set_task_specific_data(unsigned slot, void* new_value, void(*cleanup)(void*));
      class idle_guard;
   }

  class task_base : virtual public promise_base {
    public:
              void run(); 
      virtual void cancel(const char* reason FC_CANCELATION_REASON_DEFAULT_ARG) override;
      ~task_base();

      /* HERE BE DRAGONS
       *
       * Tasks are handled by an fc::thread . To avoid concurrency issues, fc::thread keeps a reference to tha
       * task in the form of a simple pointer.
       * At the same time, a task is also a promise that will be fulfilled with the task result, so typically the
       * creator of the task also keeps a reference to the task (but not necessarily always).
       *
       * Because effectively neither fc::thread nor the task creator are responsible for releasing resources
       * associated with a task, and neither can delete the task without knowing if the other still needs it,
       * the task object is managed by a shared_ptr.
       * However, fc::thread doesn't hold a shared_ptr but a native pointer. To work around this, the task can
       * be made to contain a shared_ptr holding itself (by calling retain()), which happens before the task
       * is handed to an fc::thread, e. g. in fc::async(). Once the thread has processed the task, it calls
       * release() which deletes the self-referencing shared_ptr and deletes the task object if it's no longer
       * in use anywhere.
       */
      void retain();
      void release();

    protected:
      /// Task priority looks like unsupported feature.
      uint64_t    _posted_num;
      priority    _prio;
      time_point  _when;
      void        _set_active_context(context*);
      context*    _active_context;
      task_base*  _next;

      // support for task-specific data
      std::vector<detail::specific_data_info> *_task_specific_data;

      friend void* detail::get_task_specific_data(unsigned slot);
      friend void detail::set_task_specific_data(unsigned slot, void* new_value, void(*cleanup)(void*));

      task_base(void* func);
      // opaque internal / private data used by
      // thread/thread_private
      friend class thread;
      friend class thread_d;
      friend class detail::idle_guard;
      fwd<spin_lock,8> _spinlock;

      // avoid rtti info for every possible functor...
      void*         _promise_impl;
      void*         _functor;
      void          (*_destroy_functor)(void*);
      void          (*_run_functor)(void*, void* );

      void          run_impl(); 

      void cleanup_task_specific_data();
    private:
      std::shared_ptr<promise_base> _self;
      boost::atomic<int32_t>        _retain_count;
  };

  namespace detail {
    template<typename T>
    struct functor_destructor {
      static void destroy( void* v ) { ((T*)v)->~T(); }
    };
    template<typename T>
    struct functor_run {
      static void run( void* functor, void* prom ) {
        ((promise<decltype((*((T*)functor))())>*)prom)->set_value( (*((T*)functor))() );
      }
    };
    template<typename T>
    struct void_functor_run {
      static void run( void* functor, void* prom ) {
        (*((T*)functor))();
        ((promise<void>*)prom)->set_value();
      }
    };
  }

  template<typename R,uint64_t FunctorSize=64>
  class task : virtual public task_base, virtual public promise<R> {
    public:
      typedef std::shared_ptr<task<R,FunctorSize>> ptr;

      virtual ~task(){}

      template<typename Functor>
      static ptr create( Functor&& f, const char* desc )
      {
         return ptr( new task<R,FunctorSize>( std::move(f), desc ) );
      }
      virtual void cancel(const char* reason FC_CANCELATION_REASON_DEFAULT_ARG) override { task_base::cancel(reason); }

      alignas(double) char _functor[FunctorSize];      
    private:
      template<typename Functor>
      task( Functor&& f, const char* desc ):promise_base(desc), task_base(&_functor), promise<R>(desc) {
        typedef typename fc::deduce<Functor>::type FunctorType;
        static_assert( sizeof(f) <= sizeof(_functor), "sizeof(Functor) is larger than FunctorSize" );
        new ((char*)&_functor) FunctorType( fc::forward<Functor>(f) );
        _destroy_functor = &detail::functor_destructor<FunctorType>::destroy;

        _promise_impl = static_cast<promise<R>*>(this);
        _run_functor  = &detail::functor_run<FunctorType>::run;
      }
  };

  template<uint64_t FunctorSize>
  class task<void,FunctorSize> : public task_base, public promise<void> {
    public:
      typedef std::shared_ptr<task<void,FunctorSize>> ptr;

      virtual ~task(){}
      
      template<typename Functor>
      static ptr create( Functor&& f, const char* desc )
      {
         return ptr( new task<void,FunctorSize>( std::move(f), desc ) );
      }
      virtual void cancel(const char* reason FC_CANCELATION_REASON_DEFAULT_ARG) override { task_base::cancel(reason); }

      alignas(double) char _functor[FunctorSize];
    private:
      template<typename Functor>
      task( Functor&& f, const char* desc ):promise_base(desc), task_base(&_functor), promise<void>(desc) {
        typedef typename fc::deduce<Functor>::type FunctorType;
        static_assert( sizeof(f) <= sizeof(_functor), "sizeof(Functor) is larger than FunctorSize"  );
        new ((char*)&_functor) FunctorType( fc::forward<Functor>(f) );
        _destroy_functor = &detail::functor_destructor<FunctorType>::destroy;

        _promise_impl = static_cast<promise<void>*>(this);
        _run_functor  = &detail::void_functor_run<FunctorType>::run;
      }
  };

}