#pragma once

#include <chainbase/allocator_helpers.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/core/demangle.hpp>

#include <cstring>
#include <iostream>
#include <limits>
#include <memory>

namespace chainbase {

  // BLOCK_SIZE - it means how many objects fit in allocated memory block
  template <typename T, uint32_t BLOCK_SIZE, bool PRESERVE_LAST_BLOCK>
  class pool_allocator_t : public bip::allocator<T, bip::managed_mapped_file::segment_manager>
    {
    static_assert( BLOCK_SIZE > 0 && (BLOCK_SIZE & (BLOCK_SIZE-1)) == 0, "BLOCK_SIZE should be power of 2!" );

    public:
      static constexpr uint32_t block_size = BLOCK_SIZE;

      using segment_manager_t = bip::managed_mapped_file::segment_manager;
      template <typename T2>
      using allocator_t = bip::allocator<T2, segment_manager_t>;
      using base_class_t = allocator_t<T>;

      typedef typename base_class_t::value_type value_type;
      typedef typename base_class_t::void_pointer void_pointer;
      typedef typename base_class_t::pointer pointer;
      typedef typename base_class_t::const_pointer const_pointer;
      typedef typename base_class_t::reference reference;
      typedef typename base_class_t::const_reference const_reference;
      typedef typename base_class_t::size_type size_type;
      typedef typename base_class_t::difference_type difference_type;

      template <typename U>
      struct rebind
      {
        typedef pool_allocator_t<U, BLOCK_SIZE, PRESERVE_LAST_BLOCK> other;
      };

      typedef std::false_type is_always_equal;

      pool_allocator_t(segment_manager_t* segment_manager)
        : base_class_t(segment_manager),
          block_index(typename decltype(block_index)::allocator_type(segment_manager)),
          block_list(typename decltype(block_list)::allocator_type(segment_manager))
        {
        }
      template <typename Allocator>
      pool_allocator_t(const Allocator& other)
        : base_class_t(other.get_segment_manager()),
          block_index(typename decltype(block_index)::allocator_type(other.get_segment_manager())),
          block_list(typename decltype(block_list)::allocator_type(other.get_segment_manager()))
        {
        }

      ~pool_allocator_t()
        {
        }

      pointer allocate(size_type n = 1)
        {
        assert(n == 1);
        ++allocated_count;
#ifdef CHAINBASE_POOL_LOG
        log_sampled_alloc();
#endif
        return pointer(get_object_memory());
        }

      void deallocate(pointer p, size_type n = 1)
        {
        assert(n == 1);
        --allocated_count;
#ifdef CHAINBASE_POOL_LOG
        log_sampled_dealloc(&(*p));
#endif
        return_object_memory(&(*p));
        }

#ifdef CHAINBASE_POOL_LOG
      // Sampled logging: every LOG_SAMPLE_EVERY-th call prints a one-line summary to stderr.
      // Uses per-type static counters to avoid per-call log overhead.
      static constexpr uint64_t LOG_SAMPLE_EVERY = 10000;

      void log_sampled_alloc()
        {
        static thread_local uint64_t counter = 0;
        if( (++counter % LOG_SAMPLE_EVERY) == 0 )
          {
          std::cerr << "[POOL] " << get_allocator_name()
                    << " alloc#" << counter
                    << " allocated=" << allocated_count
                    << " blocks=" << blocks_allocated_count - blocks_released_count
                    << std::endl;
          }
        }

      void log_sampled_dealloc(T* obj)
        {
        static thread_local uint64_t counter = 0;
        if( (++counter % LOG_SAMPLE_EVERY) == 0 )
          {
          std::cerr << "[POOL] " << get_allocator_name()
                    << " dealloc#" << counter
                    << " obj=" << (void*)obj
                    << " allocated=" << allocated_count
                    << " blocks=" << blocks_allocated_count - blocks_released_count
                    << std::endl;
          }
        }
#endif

      segment_manager_t* get_segment_manager() const noexcept
        {
        return base_class_t::get_segment_manager();
        }

      template <typename T2>
      auto get_generic_allocator() const
        {
        return allocator_t<T2>(get_segment_manager());
        }

      template <typename T2, uint32_t BLOCK_SIZE2 = BLOCK_SIZE>
      auto get_pool_allocator() const
        {
        return pool_allocator_t<T2, BLOCK_SIZE2, false>(get_segment_manager());
        }

      uint32_t get_block_count() const noexcept { return block_index.size(); }

      uint32_t get_allocated_count() const noexcept { return allocated_count; }

      std::string get_allocation_summary() const
        {
        std::string result(get_allocator_name() + " allocation summary:");
        result += "\nblocks: ";
        result += std::to_string(block_index.size());
        result += "\nallocated: ";
        result += std::to_string(allocated_count);
        result += "\nnon-full blocks: ";
        result += std::to_string(block_list.size());
        return result;
        }

    private:
#ifdef CHAINBASE_POOL_GUARD
      // alignas(T) + alignas(uint64_t): struct takes the strictest of T's alignment and uint64's (8).
      struct alignas(T) alignas(uint64_t) chunk_t
        {
        static constexpr uint64_t GUARD_MAGIC = 0xABCDEF0123456789ULL;
        static constexpr uint64_t GUARD_FREED = 0xFEEDDEADBEEFCAFEULL;

        union {
          char      memory[sizeof(T)];
          uint32_t  next;
        };
        uint64_t  guard_end;

        T* get_memory_address() noexcept
          {
          return reinterpret_cast<T*>(memory);
          }

        void init_guard_alloc() noexcept { guard_end = GUARD_MAGIC; }
        void init_guard_freed() noexcept { guard_end = GUARD_FREED; }
        bool check_guard_alloc() const noexcept { return guard_end == GUARD_MAGIC; }
        };
#else
      union alignas(T) chunk_t
        {
        char      memory[sizeof(T)];
        uint32_t  next;

        T* get_memory_address() noexcept
          {
          return reinterpret_cast<T*>(memory);
          }
        };
#endif

      class block_t
        {
        public:
#ifdef CHAINBASE_CHECK_SEGMENT_MANAGER
          static constexpr uint64_t CANARY_VALUE = 0xDEADBEEFCAFEBABEULL;
#endif

          block_t() noexcept
            {
#ifdef CHAINBASE_CHECK_SEGMENT_MANAGER
            canary_start = CANARY_VALUE;
            canary_end = CANARY_VALUE;
#endif
            for (uint32_t i = 1; i < BLOCK_SIZE; ++i, ++current)
              chunks[current].next = i;
            chunks[current].next = invalid_index;
            current = 0;
            }

          T* get_object_memory() noexcept
            {
            assert(!empty());
#ifdef CHAINBASE_CHECK_SEGMENT_MANAGER
            verify_canaries("get_object_memory");
#endif
#ifdef CHAINBASE_POOL_GUARD
            // If this chunk was previously allocated and freed, its guard should be in "freed" state.
            // If it was never allocated (fresh block), its guard is uninitialized — that's also fine on first alloc.
            // After giving it to the user, set the "alloc" magic.
#endif
            T* result = chunks[current].get_memory_address();
            current = chunks[current].next;
            --free_count;
#ifdef CHAINBASE_POOL_GUARD
            // Set guard to 'alloc' magic. User payload overwrites `memory` but never `guard_end`.
            reinterpret_cast<chunk_t*>(result)->init_guard_alloc();
#endif
            return result;
            }

          bool return_object_memory(T* object) noexcept
            {
            chunk_t* chunk = reinterpret_cast<chunk_t*>(object);
            assert(check_address_in_block(object) &&
              ((reinterpret_cast<std::size_t>(chunk) - reinterpret_cast<std::size_t>(chunks)) % sizeof(chunk_t)) == 0);
#ifdef CHAINBASE_CHECK_SEGMENT_MANAGER
            verify_canaries("return_object_memory");
#endif
#ifdef CHAINBASE_POOL_GUARD
            // The chunk should still have its 'alloc' guard magic. If it's corrupted,
            // something wrote past sizeof(T) into the guard area.
            if (!chunk->check_guard_alloc())
              {
              std::cerr << "!!! POOL GUARD CORRUPTION in " << get_allocator_name()
                        << " at return_object_memory"
                        << " chunk=" << (void*)chunk
                        << " guard=0x" << std::hex << chunk->guard_end << std::dec
                        << " expected=0x" << std::hex << chunk_t::GUARD_MAGIC << std::dec
                        << " => something wrote past sizeof(T)=" << sizeof(T)
                        << std::endl;
              std::cerr.flush();
              abort();
              }
#endif
            chunk->next = current;
            current = chunk - chunks;
#ifdef CHAINBASE_POISON_FREED_MEMORY
            // Poison freed memory after the free-list pointer to detect use-after-free.
            // The first sizeof(uint32_t) bytes hold the free-list 'next' pointer; fill the rest with 0xDE.
            constexpr size_t poison_offset = sizeof(uint32_t);
            if constexpr( sizeof(T) > poison_offset )
              std::memset( chunk->memory + poison_offset, 0xDE, sizeof(T) - poison_offset );
#endif
#ifdef CHAINBASE_POOL_GUARD
            chunk->init_guard_freed();
#endif
            return ++free_count == BLOCK_SIZE;
            }

          bool check_address_in_block(T* object) const noexcept
            {
            const chunk_t* chunk = reinterpret_cast<chunk_t*>(object);
            return chunk >= chunks && chunk < (chunks + BLOCK_SIZE);
            }

          void set_on_list_index(uint32_t index) noexcept
            {
            on_list_index = index;
            }

          void reset_on_list_index() noexcept
            {
            on_list_index = invalid_index;
            }

          bool is_on_list() const noexcept
            {
            return on_list_index != invalid_index;
            }

          uint32_t get_on_list_index() const noexcept
            {
            return on_list_index;
            }

          // true if whole memory is free
          bool full() const noexcept
            {
            return free_count == BLOCK_SIZE;
            }

          // true if no more free memory
          bool empty() const noexcept
            {
            return free_count == 0;
            }

          const chunk_t* get_chunks_address() const noexcept
            {
            return chunks;
            }

          const void* get_block_start() const noexcept
            {
            return reinterpret_cast<const void*>(this);
            }

          const void* get_block_end() const noexcept
            {
            return reinterpret_cast<const char*>(this) + sizeof(block_t);
            }

#ifdef CHAINBASE_CHECK_SEGMENT_MANAGER
          void verify_canaries(const char* context) const
            {
            if (canary_start != CANARY_VALUE || canary_end != CANARY_VALUE)
              {
              std::cerr << "!!! BLOCK CANARY CORRUPTION in " << get_allocator_name()
                        << " at " << context
                        << " block=" << (const void*)this
                        << " canary_start=" << std::hex << canary_start
                        << " canary_end=" << canary_end << std::dec
                        << " expected=" << std::hex << CANARY_VALUE << std::dec
                        << std::endl;
              std::cerr.flush();
              abort();
              }
            }
#endif

          static constexpr uint32_t invalid_index = std::numeric_limits<uint32_t>::max();

        private:
#ifdef CHAINBASE_CHECK_SEGMENT_MANAGER
          uint64_t  canary_start = CANARY_VALUE;
#endif
          chunk_t   chunks[BLOCK_SIZE];
          uint32_t  current = 0;
          uint32_t  free_count = BLOCK_SIZE;
          uint32_t  on_list_index = invalid_index;
#ifdef CHAINBASE_CHECK_SEGMENT_MANAGER
          uint64_t  canary_end = CANARY_VALUE;
#endif
        };

      struct block_comparator_t
        {
        bool operator () (const block_t& b1, const block_t& b2) const noexcept
          {
          return b1.get_chunks_address() > b2.get_chunks_address();
          }
        };

      using block_ptr_t = bip::offset_ptr<block_t>;
      using block_index_t = bip::set<block_t, block_comparator_t, bip::allocator<block_t, segment_manager_t>>;
      using block_list_t = bip::vector<block_ptr_t, bip::allocator<block_ptr_t, segment_manager_t>>;

    private:
      T* get_object_memory()
        {
        while (current_block == nullptr && !block_list.empty())
          current_block = pop_from_block_list();

        if (current_block == nullptr)
          current_block = allocate_block();
        else
          current_block->reset_on_list_index();

        T* result = current_block->get_object_memory();

        if (current_block->empty())
          current_block = nullptr;

        return result;
        }

      void return_object_memory(T* object)
        {
        block_t* block;

        if (current_block != nullptr && current_block->check_address_in_block(object))
          {
          block = &(*current_block);
          }
        else if (last_found_block != nullptr && last_found_block->check_address_in_block(object))
          {
          block = &(*last_found_block);
          }
        else
          {
          block = reinterpret_cast<block_t*>(object);
          const auto found = block_index.lower_bound(*block);
          assert(found != block_index.cend());
          block = const_cast<block_t*>(&(*found));
          last_found_block = block;
          }

        block->return_object_memory(object);
        // Never release blocks back to segment_manager. Keep empty blocks in
        // the pool for reuse. Releasing blocks causes use-after-free when
        // segment_manager reuses the memory while tree node pointers still
        // reference addresses within the released block.
        if (block != current_block)
          {
          push_to_block_list(block);
          }
        }

      block_ptr_t allocate_block()
        {
        auto insert_result = block_index.emplace();
        ++blocks_allocated_count;
        block_ptr_t block = const_cast<block_t*>(&(*insert_result.first));
#ifdef CHAINBASE_CHECK_SEGMENT_MANAGER
        check_block_overlap(insert_result.first);
#endif
#ifdef CHAINBASE_POOL_LOG
        std::cerr << "[POOL] " << get_allocator_name()
                  << " allocate_block#" << blocks_allocated_count
                  << " block=" << (void*)(&(*block))
                  << " size=" << sizeof(block_t)
                  << " [" << (void*)block->get_block_start()
                  << ", " << (void*)block->get_block_end() << ")"
                  << std::endl;
#endif
        return block;
        }

#ifdef CHAINBASE_CHECK_SEGMENT_MANAGER
      void check_block_overlap(typename block_index_t::iterator it)
        {
        const block_t& new_block = *it;
        const char* new_start = reinterpret_cast<const char*>(new_block.get_block_start());
        const char* new_end = reinterpret_cast<const char*>(new_block.get_block_end());

        // block_index is sorted by descending address, so:
        // - prev (toward begin) has HIGHER address
        // - next (toward end) has LOWER address

        // Check prev (higher address): overlap if new_end extends up into prev
        if (it != block_index.begin())
          {
          auto prev = std::prev(it);
          const char* prev_start = reinterpret_cast<const char*>(prev->get_block_start());
          if (new_end > prev_start)
            {
            std::cerr << "!!! SEGMENT MANAGER CORRUPTION: OVERLAPPING BLOCKS in " << get_allocator_name()
                      << "\n  new  block: [" << (const void*)new_start
                      << ", " << (const void*)new_end << ")"
                      << "\n  prev block: [" << (const void*)prev_start
                      << ", " << (const void*)prev->get_block_end() << ")"
                      << "\n  overlap: " << (new_end - prev_start) << " bytes"
                      << "\n  blocks_allocated: " << blocks_allocated_count
                      << "\n  blocks_released: " << blocks_released_count
                      << std::endl;
            std::cerr.flush();
            abort();
            }
          }

        // Check next (lower address): overlap if next_end extends up into new
        auto next = std::next(it);
        if (next != block_index.end())
          {
          const char* next_end = reinterpret_cast<const char*>(next->get_block_end());
          if (next_end > new_start)
            {
            std::cerr << "!!! SEGMENT MANAGER CORRUPTION: OVERLAPPING BLOCKS in " << get_allocator_name()
                      << "\n  next block: [" << (const void*)next->get_block_start()
                      << ", " << (const void*)next_end << ")"
                      << "\n  new  block: [" << (const void*)new_start
                      << ", " << (const void*)new_end << ")"
                      << "\n  overlap: " << (next_end - new_start) << " bytes"
                      << "\n  blocks_allocated: " << blocks_allocated_count
                      << "\n  blocks_released: " << blocks_released_count
                      << std::endl;
            std::cerr.flush();
            abort();
            }
          }
        }
#endif

      void push_to_block_list(block_t* block)
        {
        if (!block->is_on_list())
          {
          block->set_on_list_index(static_cast<uint32_t>(block_list.size()));
          block_list.emplace_back(block);
          ++blocks_pushed_count;
          }
        }

      block_ptr_t pop_from_block_list() noexcept
        {
        assert(!block_list.empty());
        block_ptr_t block = block_list.back();
        block_list.pop_back();
        if( block )
          ++blocks_popped_count;
        return block;
        }

    public:
      static std::string get_type_name()
        {
        if constexpr (helpers::type_traits::has_value_type_member_v<T>)
          return boost::core::demangle(typeid(typename T::value_type).name());
        else
          return boost::core::demangle(typeid(T).name());
        }

      static std::string get_allocator_name()
        {
        return std::string("pool_allocator<") + get_type_name() + ">";
        }

    private:
      block_index_t             block_index;
      block_list_t              block_list;
      block_ptr_t               current_block = nullptr;
      block_ptr_t               last_found_block = nullptr;
      uint32_t                  allocated_count = 0;

    public:
      static size_t             blocks_allocated_count; /// ABW: note that multi_index allocates extra node inside constructor (in some index configurations)
      static size_t             blocks_released_count; /// ABW: for above reason not all blocks are going to be released, unless multi_index is destroyed
      /// number of times first object was released from block that was not current_block
      static size_t             blocks_pushed_count;
      /// number of times first object was acquired from partially filled block that was not current_block (== how many times current_block switched to different partially allocated block)
      static size_t             blocks_popped_count;
    };

  template <typename T, uint32_t BLOCK_SIZE, bool PRESERVE_LAST_BLOCK>
  size_t pool_allocator_t<T, BLOCK_SIZE, PRESERVE_LAST_BLOCK>::blocks_allocated_count = 0;
  template <typename T, uint32_t BLOCK_SIZE, bool PRESERVE_LAST_BLOCK>
  size_t pool_allocator_t<T, BLOCK_SIZE, PRESERVE_LAST_BLOCK>::blocks_released_count = 0;
  template <typename T, uint32_t BLOCK_SIZE, bool PRESERVE_LAST_BLOCK>
  size_t pool_allocator_t<T, BLOCK_SIZE, PRESERVE_LAST_BLOCK>::blocks_pushed_count = 0;
  template <typename T, uint32_t BLOCK_SIZE, bool PRESERVE_LAST_BLOCK>
  size_t pool_allocator_t<T, BLOCK_SIZE, PRESERVE_LAST_BLOCK>::blocks_popped_count = 0;

  template <typename T, uint32_t BLOCK_SIZE, bool PRESERVE_LAST_BLOCK>
  constexpr uint32_t pool_allocator_t<T, BLOCK_SIZE, PRESERVE_LAST_BLOCK>::block_t::invalid_index;

  template <typename POOL_ALLOCATOR>
  class shared_pool_allocator_t;

  /**
    * Helper class (posing as allocator) to transport shared pool_allocator_t instance into final shared_pool_allocator_t.
    * map/set requires specific allocator to be passed to their constructor and that is different from what is then internally
    * created and used. It means those allocators are only created for the constructor call and immediately destroyed.
    * They are never used. Hence it does not even provide allocation related methods.
    */
  template <typename T, uint32_t BLOCK_SIZE>
  class shared_allocator_carrier
  {
  public:
    template<typename U>
    using impl_allocator_t = pool_allocator_t<U, BLOCK_SIZE, true>;

    using value_type = typename impl_allocator_t<T>::value_type;
    using void_pointer = typename impl_allocator_t<T>::void_pointer;

    template <typename U>
    struct rebind
    {
      typedef shared_pool_allocator_t<impl_allocator_t<U>> other;
    };

    typedef std::false_type is_always_equal;

    template <typename U>
    shared_allocator_carrier( impl_allocator_t<U>& to_carry )
      : carried_allocator( &to_carry ) {}

    ~shared_allocator_carrier() {}

    template <typename U>
    impl_allocator_t<U>* get_carried_allocator() const { return reinterpret_cast< impl_allocator_t<U>* >( carried_allocator ); }

  private:
    void* carried_allocator; // final type is only known after rebind, so we can't store it in typed version
  };

  /**
    * Uses given instance of pool allocator (created outside, typically as part of generic_index), as its implementation.
    * You can pass the same instance of pool allocator to many instances of this class, effectively sharing it.
    */
  template <typename POOL_ALLOCATOR>
  class shared_pool_allocator_t
  {
  public:
    using value_type = typename POOL_ALLOCATOR::value_type;
    using void_pointer = typename POOL_ALLOCATOR::void_pointer;
    using pointer = typename POOL_ALLOCATOR::pointer;
    using const_pointer = typename POOL_ALLOCATOR::const_pointer;
    using reference = typename POOL_ALLOCATOR::reference;
    using const_reference = typename POOL_ALLOCATOR::const_reference;
    using size_type = typename POOL_ALLOCATOR::size_type;
    using difference_type = typename POOL_ALLOCATOR::difference_type;

    typedef std::false_type is_always_equal;

    template <typename T>
    shared_pool_allocator_t( const shared_allocator_carrier<T, POOL_ALLOCATOR::block_size>& carrier )
      : impl( carrier.template get_carried_allocator<value_type>() ) {}

    ~shared_pool_allocator_t() {}

    pointer allocate( size_type n = 1 ) { return impl->allocate( n ); }
    void deallocate( pointer p, size_type n = 1 ) { impl->deallocate( p, n ); }

  private:
    using impl_ptr_t = bip::offset_ptr<POOL_ALLOCATOR>;

    impl_ptr_t impl;
  };

  constexpr uint32_t DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE = 1 << 12; // 4k

  constexpr uint32_t DEFAULT_UNDO_STATE_POOL_ALLOCATOR_BLOCK_SIZE = 1 << 8; // 256

  template <typename T, uint32_t BLOCK_SIZE = DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE>
  using multi_index_allocator = pool_allocator_t<T, BLOCK_SIZE, false>;

  template <typename T, uint32_t BLOCK_SIZE = DEFAULT_UNDO_STATE_POOL_ALLOCATOR_BLOCK_SIZE>
  using undo_state_allocator = pool_allocator_t<T, BLOCK_SIZE, true>;

  template <typename T, uint32_t BLOCK_SIZE = DEFAULT_UNDO_STATE_POOL_ALLOCATOR_BLOCK_SIZE>
  using undo_allocator_carrier = shared_allocator_carrier<T, BLOCK_SIZE>;

} // namespace chainbase
