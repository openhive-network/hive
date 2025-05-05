#pragma once

#include <chainbase/allocator_helpers.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/core/demangle.hpp>

#include <limits>

namespace chainbase {

  // BLOCK_SIZE - it means how many objects fit in allocated memory block
  template <typename T, uint32_t BLOCK_SIZE, bool USE_MANAGED_MAPPED_FILE = !_ENABLE_STD_ALLOCATOR>
  class pool_allocator_t : private std::conditional_t<USE_MANAGED_MAPPED_FILE, bip::allocator<T, bip::managed_mapped_file::segment_manager>, std::allocator<T>>
    {
    static_assert((BLOCK_SIZE & (BLOCK_SIZE-1)) == 0, "BLOCK_SIZE should be power of 2!");

    private:
      using segment_manager_t = std::conditional_t<USE_MANAGED_MAPPED_FILE, bip::managed_mapped_file::segment_manager, void>;
      template <typename T2>
      using allocator_t = std::conditional_t<USE_MANAGED_MAPPED_FILE, bip::allocator<T2, segment_manager_t>, std::allocator<T2>>;
      using base_class_t = allocator_t<T>;

    public:
      typedef typename base_class_t::value_type value_type;
      typedef typename base_class_t::pointer pointer;
      typedef typename base_class_t::const_pointer const_pointer;
      typedef typename base_class_t::reference reference;
      typedef typename base_class_t::const_reference const_reference;
      typedef typename base_class_t::size_type size_type;
      typedef typename base_class_t::difference_type difference_type;

      template <typename U>
      struct rebind
      {
        typedef pool_allocator_t<U, BLOCK_SIZE> other;
      };

      typedef std::false_type is_always_equal;

    public:
      template <bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(segment_manager_t* segment_manager, std::enable_if_t<_USE_MANAGED_MAPPED_FILE>* = nullptr)
        : base_class_t(segment_manager),
          block_index(typename decltype(block_index)::allocator_type(segment_manager)),
          block_list(typename decltype(block_list)::allocator_type(segment_manager))
        {
        }
      template <bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(std::enable_if_t<!_USE_MANAGED_MAPPED_FILE>* = nullptr)
        {
        }
      template <typename T2, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(const pool_allocator_t<T2, BLOCK_SIZE, USE_MANAGED_MAPPED_FILE>& other,
        std::enable_if_t<_USE_MANAGED_MAPPED_FILE>* = nullptr)
        : base_class_t(other.get_segment_manager()),
          block_index(typename decltype(block_index)::allocator_type(other.get_segment_manager())),
          block_list(typename decltype(block_list)::allocator_type(other.get_segment_manager()))
        {
        }
      template <typename T2, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(const pool_allocator_t<T2, BLOCK_SIZE, USE_MANAGED_MAPPED_FILE>& other,
        std::enable_if_t<!_USE_MANAGED_MAPPED_FILE>* = nullptr)
        {
        }

      ~pool_allocator_t()
        {
        }

      pointer allocate(size_type n = 1)
        {
        assert(n == 1);
        ++allocated_count;
        return pointer(get_object_memory());
        }

      void deallocate(pointer p, size_type n = 1)
        {
        assert(n == 1);
        --allocated_count;
        return_object_memory(&(*p));
        }

      segment_manager_t* get_segment_manager() const noexcept
        {
        if constexpr (USE_MANAGED_MAPPED_FILE)
          return base_class_t::get_segment_manager();
        else
          return nullptr;
        }

      template <typename T2>
      auto get_generic_allocator() const
        {
        if constexpr (USE_MANAGED_MAPPED_FILE)
          return allocator_t<T2>(get_segment_manager());
        else
          return allocator_t<T2>();
        }

      template <typename T2, uint32_t BLOCK_SIZE2 = BLOCK_SIZE>
      auto get_pool_allocator() const
        {
        if constexpr (USE_MANAGED_MAPPED_FILE)
          return pool_allocator_t<T2, BLOCK_SIZE2, true>(get_segment_manager());
        else
          return pool_allocator_t<T2, BLOCK_SIZE2, false>();
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
      union alignas(T) chunk_t
        {
        char      memory[sizeof(T)];
        uint32_t  next;

        T* get_memory_address() noexcept
          {
          return reinterpret_cast<T*>(memory);
          }
        };

      class block_t
        {
        public:
          block_t() noexcept
            {
            for (uint32_t i = 1; i < BLOCK_SIZE; ++i, ++current)
              chunks[current].next = i;
            chunks[current].next = 0;
            current = 0;
            }

          T* get_object_memory() noexcept
            {
            assert(!empty());
            T* result = chunks[current].get_memory_address();
            current = chunks[current].next;
            --free_count;
            return result;
            }

          bool return_object_memory(T* object) noexcept
            {
            chunk_t* chunk = reinterpret_cast<chunk_t*>(object);
            assert(check_address_in_block(object) &&
              ((reinterpret_cast<std::size_t>(chunk) - reinterpret_cast<std::size_t>(chunks)) % sizeof(T)) == 0);
            chunk->next = current;
            current = chunk - chunks;
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
            on_list_index = invalid_list_index;
            }

          bool is_on_list() const noexcept
            {
            return on_list_index != invalid_list_index;
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

          static constexpr uint32_t invalid_list_index = std::numeric_limits<uint32_t>::max();

        private:
          chunk_t   chunks[BLOCK_SIZE];
          uint32_t  current = 0;
          uint32_t  free_count = BLOCK_SIZE;
          uint32_t  on_list_index = invalid_list_index;
        };

      struct block_comparator_t
        {
        bool operator () (const block_t& b1, const block_t& b2) const noexcept
          {
          return b1.get_chunks_address() > b2.get_chunks_address();
          }
        };

      using block_ptr_t = std::conditional_t<USE_MANAGED_MAPPED_FILE, bip::offset_ptr<block_t>, block_t*>;
      using block_index_t = std::conditional_t<USE_MANAGED_MAPPED_FILE,
        bip::set<block_t, block_comparator_t, bip::allocator<block_t, segment_manager_t>>,
        std::set<block_t, block_comparator_t>>;
      using block_list_t = std::conditional_t<USE_MANAGED_MAPPED_FILE,
        bip::vector<block_ptr_t, bip::allocator<block_ptr_t, segment_manager_t>>,
        std::vector<block_ptr_t>>;

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

        if (block->return_object_memory(object))
          {
          if (block != current_block)
            {
            const auto index = block->get_on_list_index();

            if (index == (block_list.size()-1))
              block_list.pop_back();
            else
              block_list[index] = nullptr;
            }
          else
            {
            current_block = nullptr;
            }

          if (block == last_found_block)
            last_found_block = nullptr;

          block_index.erase(*block);
          ++blocks_released_count;
          }
        else if (block != current_block)
          {
          push_to_block_list(block);
          }
        }

      block_ptr_t allocate_block()
        {
        auto insert_result = block_index.emplace();
        ++blocks_allocated_count;
        block_ptr_t block = const_cast<block_t*>(&(*insert_result.first));
        return block;
        }

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

      std::string get_type_name() const
        {
        if constexpr (helpers::type_traits::has_value_type_member_v<T>)
          return boost::core::demangle(typeid(typename T::value_type).name());
        else
          return boost::core::demangle(typeid(T).name());
        }

      std::string get_allocator_name() const
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
      static constexpr uint32_t block_size = BLOCK_SIZE;
      static size_t             blocks_allocated_count; /// ABW: note that multi_index allocates extra node inside constructor (in some index configurations)
      static size_t             blocks_released_count; /// ABW: for above reason not all blocks are going to be released, unless multi_index is destroyed
      /// number of times first object was released from block that was not current_block
      static size_t             blocks_pushed_count;
      /// number of times first object was acquired from partially filled block that was not current_block (== how many times current_block switched to different partially allocated block)
      static size_t             blocks_popped_count;
    };

  template <typename T, uint32_t BLOCK_SIZE, bool USE_MANAGED_MAPPED_FILE>
  size_t pool_allocator_t<T, BLOCK_SIZE, USE_MANAGED_MAPPED_FILE>::blocks_allocated_count = 0;
  template <typename T, uint32_t BLOCK_SIZE, bool USE_MANAGED_MAPPED_FILE>
  size_t pool_allocator_t<T, BLOCK_SIZE, USE_MANAGED_MAPPED_FILE>::blocks_released_count = 0;
  template <typename T, uint32_t BLOCK_SIZE, bool USE_MANAGED_MAPPED_FILE>
  size_t pool_allocator_t<T, BLOCK_SIZE, USE_MANAGED_MAPPED_FILE>::blocks_pushed_count = 0;
  template <typename T, uint32_t BLOCK_SIZE, bool USE_MANAGED_MAPPED_FILE>
  size_t pool_allocator_t<T, BLOCK_SIZE, USE_MANAGED_MAPPED_FILE>::blocks_popped_count = 0;

  template <typename T, uint32_t BLOCK_SIZE, bool USE_MANAGED_MAPPED_FILE>
    constexpr uint32_t pool_allocator_t<T, BLOCK_SIZE, USE_MANAGED_MAPPED_FILE>::block_t::invalid_list_index;

  constexpr uint32_t DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE = 1 << 12; // 4k

  constexpr uint32_t DEFAULT_UNDO_STATE_POOL_ALLOCATOR_BLOCK_SIZE = 1 << 5; // 32

  template <typename T, uint32_t BLOCK_SIZE = DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE>
  using multi_index_allocator = std::conditional_t<_ENABLE_MULTI_INDEX_POOL_ALLOCATOR,
    //bip::adaptive_pool<T, bip::managed_mapped_file::segment_manager, BLOCK_SIZE>, <-- ABW: it is way slower
    //  than pool_allocator_t, especially for large amounts of data that we have in mainnet
    pool_allocator_t<T, BLOCK_SIZE>,
    allocator<T> >;

  template <typename T>
  using undo_state_allocator = std::conditional_t<_ENABLE_UNDO_STATE_POOL_ALLOCATOR,
    pool_allocator_t<T, DEFAULT_UNDO_STATE_POOL_ALLOCATOR_BLOCK_SIZE>,
    allocator<T> >;

} // namespace chainbase
