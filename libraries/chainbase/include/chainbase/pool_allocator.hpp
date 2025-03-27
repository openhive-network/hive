#pragma once

#include <chainbase/allocator_helpers.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/core/demangle.hpp>

namespace chainbase {

  using namespace boost::multi_index;

  const uint32_t DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE = 1 << 12; // 4k
  //constexpr uint32_t DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE = 1 << 5; // 32

#if defined(ENABLE_MULTI_INDEX_POOL_ALLOCATOR)
  static_assert((DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE & (DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE-1)) == 0,
    "DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE should be power of 2!");
#endif

  //const uint32_t DEFAULT_UNDO_STATE_POOL_ALLOCATOR_BLOCK_SIZE = 1 << 12; // 4k
  constexpr uint32_t DEFAULT_UNDO_STATE_POOL_ALLOCATOR_BLOCK_SIZE = 1 << 5; // 32

#if defined(ENABLE_UNDO_STATE_POOL_ALLOCATOR)
  static_assert((DEFAULT_UNDO_STATE_POOL_ALLOCATOR_BLOCK_SIZE & (DEFAULT_UNDO_STATE_POOL_ALLOCATOR_BLOCK_SIZE-1)) == 0,
    "DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE should be power of 2!");
#endif

  using segment_manager_t = std::conditional_t<
    _ENABLE_STD_ALLOCATOR,
    void,
    bip::managed_mapped_file::segment_manager>;

  // BLOCK_SIZE - it means how many objects fit in allocated memory block
  template <typename T, uint32_t BLOCK_SIZE, typename TSegmentManager = segment_manager_t, bool USE_MANAGED_MAPPED_FILE = !_ENABLE_STD_ALLOCATOR>
  class pool_allocator_t : private allocator<T>
    {
    private:
      typedef allocator<T> base_class_t;

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
      pool_allocator_t(TSegmentManager* _segment_manager, std::enable_if_t<_USE_MANAGED_MAPPED_FILE>* = nullptr)
        : base_class_t(_segment_manager),
          block_index(typename decltype(block_index)::allocator_type(_segment_manager)),
          block_list(typename decltype(block_list)::allocator_type(_segment_manager))
        {
        }
      template <bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(std::enable_if_t<!_USE_MANAGED_MAPPED_FILE>* = nullptr)
        {
        }
      template <typename T2, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(const pool_allocator_t<T2, BLOCK_SIZE, TSegmentManager, USE_MANAGED_MAPPED_FILE>& other,
        std::enable_if_t<_USE_MANAGED_MAPPED_FILE>* = nullptr)
        : base_class_t(other.get_segment_manager()),
          block_index(typename decltype(block_index)::allocator_type(other.get_segment_manager())),
          block_list(typename decltype(block_list)::allocator_type(other.get_segment_manager()))
        {
        }
      template <typename T2, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(const pool_allocator_t<T2, BLOCK_SIZE, TSegmentManager, USE_MANAGED_MAPPED_FILE>& other,
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

      template <bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      segment_manager_t* get_segment_manager(std::enable_if_t<_USE_MANAGED_MAPPED_FILE>* = nullptr) const noexcept
        {
        return base_class_t::get_segment_manager();
        }

      template <bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      segment_manager_t* get_segment_manager(std::enable_if_t<!_USE_MANAGED_MAPPED_FILE>* = nullptr) const noexcept
        {
        return nullptr;
        }

      template <typename T2, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      auto get_generic_allocator(std::enable_if_t<_USE_MANAGED_MAPPED_FILE>* = nullptr) const
        {
        return allocator<T2>(get_segment_manager());
        }

      template <typename T2, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      auto get_generic_allocator(std::enable_if_t<!_USE_MANAGED_MAPPED_FILE>* = nullptr) const
        {
        return allocator<T2>();
        }

      template <typename T2, uint32_t BLOCK_SIZE2 = BLOCK_SIZE, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      auto get_pool_allocator(std::enable_if<_USE_MANAGED_MAPPED_FILE>* = nullptr) const
        {
        return pool_allocator_t<T2, BLOCK_SIZE2, TSegmentManager, USE_MANAGED_MAPPED_FILE>(get_segment_manager());
        }

      template <typename T2, uint32_t BLOCK_SIZE2 = BLOCK_SIZE, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      auto get_pool_allocator(std::enable_if<!_USE_MANAGED_MAPPED_FILE>* = nullptr) const
        {
        return pool_allocator_t<T2, BLOCK_SIZE2, TSegmentManager, USE_MANAGED_MAPPED_FILE>();
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
            chunk_t* chunk = reinterpret_cast<chunk_t*>(object);
            return chunk >= chunks && chunk < (chunks + BLOCK_SIZE);
            }

          void set_on_list(bool value) noexcept
            {
            on_list = value;
            }

          bool is_on_list() const noexcept
            {
            return on_list;
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

        private:
          chunk_t   chunks[BLOCK_SIZE];
          uint32_t  current = 0;
          uint32_t  free_count = BLOCK_SIZE;
          bool      on_list = false;
        };

      using block_ptr_t = std::conditional_t<_ENABLE_STD_ALLOCATOR, block_t*, bip::offset_ptr<block_t>>;

      struct block_comparator_t
        {
        bool operator () (const block_t& b1, const block_t& b2) const noexcept
          {
          return b1.get_chunks_address() > b2.get_chunks_address();
          }
        };

      using block_index_t = t_set<block_t, block_comparator_t>;
      using block_list_t = t_vector<block_ptr_t>;

    private:
      T* get_object_memory()
        {
        if (current_block == nullptr)
          {
          while (!block_list.empty())
            {
            block_ptr_t block = remove_from_block_list();

            if (block->full() && !block_list.empty())
              {
              block_index.erase(*block);
              }
            else
              {
              current_block = block;
              break;
              }
            }

          if (current_block == nullptr)
            current_block = allocate_block();
          }

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
        else
          {
          block = reinterpret_cast<block_t*>(object);
          const auto found = block_index.lower_bound(*block);
          assert(found != block_index.cend());
          block = const_cast<block_t*>(&(*found));
          add_to_block_list(block);
          }

        block->return_object_memory(object);
        }

      block_ptr_t allocate_block()
        {
        auto insert_result = block_index.emplace();
        block_ptr_t block = const_cast<block_t*>(&(*insert_result.first));
        return block;
        }

      void add_to_block_list(block_t* block)
        {
        if (!block->is_on_list())
          {
          block->set_on_list(true);
          block_list.emplace_back(block);
          }
        }

      block_ptr_t remove_from_block_list() noexcept
        {
        assert(!block_list.empty());
        block_ptr_t block = block_list.back();
        block->set_on_list(false);
        block_list.pop_back();
        return block;
        }

      template <typename _T = T>
      std::string get_type_name(std::enable_if_t<helpers::type_traits::has_value_type_member_v<_T>>* = nullptr) const
        {
        return boost::core::demangle(typeid(typename _T::value_type).name());
        }

      template <typename _T = T>
      std::string get_type_name(std::enable_if_t<!helpers::type_traits::has_value_type_member_v<_T>>* = nullptr) const
        {
        return boost::core::demangle(typeid(_T).name());
        }

      std::string get_allocator_name() const
        {
        return std::string("pool_allocator<") + get_type_name() + ">";
        }

    private:
      block_index_t             block_index;
      block_list_t              block_list;
      block_ptr_t               current_block = nullptr;
      uint32_t                  allocated_count = 0;
    };

  template <typename T>
  using multi_index_allocator = std::conditional_t<_ENABLE_MULTI_INDEX_POOL_ALLOCATOR,
    pool_allocator_t<T, DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE>,
    allocator<T> >;

  template <typename T>
  using undo_state_allocator = std::conditional_t<_ENABLE_UNDO_STATE_POOL_ALLOCATOR,
    pool_allocator_t<T, DEFAULT_UNDO_STATE_POOL_ALLOCATOR_BLOCK_SIZE>,
    allocator<T> >;

} // namespace chainbase
