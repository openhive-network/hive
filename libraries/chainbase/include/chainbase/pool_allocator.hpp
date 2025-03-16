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
  class pool_allocator_t
    {
    public:
      typedef T value_type;
      typedef T* pointer;
      typedef const T* const_pointer;
      typedef T& reference;
      typedef const T& const_reference;
      typedef std::size_t size_type;
      typedef std::ptrdiff_t difference_type;
      typedef std::true_type propagate_on_container_move_assignment;

      template <typename U>
      struct rebind
      {
        typedef pool_allocator_t<U, BLOCK_SIZE> other;
      };

      typedef std::true_type is_always_equal;

    public:
      template <bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(TSegmentManager* _segment_manager, std::enable_if_t<_USE_MANAGED_MAPPED_FILE>* = nullptr)
        : block_index(typename decltype(block_index)::allocator_type(_segment_manager)),
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
        : block_index(typename decltype(block_index)::allocator_type(other.get_segment_manager())),
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

      T* allocate(size_type n = 1)
        {
        assert(n == 1);
        ++allocated_count;
        return get_object_memory();
        }

      void deallocate(T* p, size_type n = 1)
        {
        assert(n == 1);
        --allocated_count;
        return_object_memory(p);
        }

      template <bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      segment_manager_t* get_segment_manager(std::enable_if_t<_USE_MANAGED_MAPPED_FILE>* = nullptr) const
        {
        return block_index.get_allocator().get_segment_manager();
        }

      template <bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      segment_manager_t* get_segment_manager(std::enable_if_t<!_USE_MANAGED_MAPPED_FILE>* = nullptr) const
        {
        return nullptr;
        }

      template <typename T2, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      allocator<T2> get_generic_allocator(std::enable_if_t<_USE_MANAGED_MAPPED_FILE>* = nullptr) const
        {
        return allocator<T2>(get_segment_manager());
        }

      template <typename T2, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      allocator<T2> get_generic_allocator(std::enable_if_t<!_USE_MANAGED_MAPPED_FILE>* = nullptr) const
        {
        return allocator<T2>();
        }

      template <typename T2, uint32_t BLOCK_SIZE2 = BLOCK_SIZE, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t<T2, BLOCK_SIZE2, TSegmentManager, USE_MANAGED_MAPPED_FILE> get_pool_allocator(std::enable_if<_USE_MANAGED_MAPPED_FILE>* = nullptr) const
        {
        return pool_allocator_t<T2, BLOCK_SIZE2, TSegmentManager, USE_MANAGED_MAPPED_FILE>(get_segment_manager());
        }

      template <typename T2, uint32_t BLOCK_SIZE2 = BLOCK_SIZE, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t<T2, BLOCK_SIZE2, TSegmentManager, USE_MANAGED_MAPPED_FILE> get_pool_allocator(std::enable_if<!_USE_MANAGED_MAPPED_FILE>* = nullptr) const
        {
        return pool_allocator_t<T2, BLOCK_SIZE2, TSegmentManager, USE_MANAGED_MAPPED_FILE>();
        }

      uint32_t get_block_count() const { return block_index.size(); }

      uint32_t get_allocated_count() const { return allocated_count; }

      std::string get_allocation_summary() const
        {
        std::string result(get_allocator_name() + " allocation summary:");
        result += "\nblocks: ";
        result += std::to_string(block_index.size());
        result += "\nallocated: ";
        result += std::to_string(allocated_count);
        result += "\nnon-full blocks: ";
        result += std::to_string(block_list_size);
        return result;
        }

    private:
      union chunk_t
        {
        char      object_memory[sizeof(T)];
        chunk_t*  next;

        T* get_object_memory()
          {
          return reinterpret_cast<T*>(object_memory);
          }
        };

      struct block_t
        {
        chunk_t   chunks[BLOCK_SIZE];
        chunk_t*  current = nullptr;
        uint32_t  free_count = BLOCK_SIZE;
        bool      on_list = false;

        block_t()
          {
          current = chunks;
          for (uint32_t i = 1; i < BLOCK_SIZE; ++i, ++current)
            current->next = chunks + i;
          current->next = nullptr;
          print();
          current = chunks;
          }

        void print()
          {
          return;
          /*std::cout << chunks << std::endl;
          for (int i = 0; i < BLOCK_SIZE; ++i)
            std::cout << chunks[i].next << std::endl;*/
          }

        T* get_object_memory()
          {
          print();
          assert(current != nullptr);
          T* result = current->get_object_memory();
          current = current->next;
          --free_count;
          assert((current == nullptr) == empty());
          return result;
          }

        bool return_object_memory(chunk_t* chunk)
          {
          assert(chunk >= get_chunks_address() && chunk < (get_chunks_address()+BLOCK_SIZE));
          chunk->next = current;
          current = chunk;
          return ++free_count == BLOCK_SIZE;
          }

        const chunk_t* get_chunks_address() const
          {
          return chunks;
          }

        // true if whole memory is free
        bool full() const
          {
          return free_count == BLOCK_SIZE;
          }

        // true if no more free memory
        bool empty() const
          {
          return free_count == 0;
          }
        };

      struct by_address;
      using block_index_t = multi_index_container<
        block_t,
        indexed_by<
          ordered_unique<tag<by_address>,
            const_mem_fun<block_t, const chunk_t*, &block_t::get_chunks_address>,
            std::greater<const chunk_t*>
          >
        >,
        allocator<block_t>
      >;
      using block_list_t = forward_list_t<block_t*>;

    private:
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

      T* get_object_memory()
        {
        if (current_block == nullptr)
          {
          while (!block_list.empty())
            {
            block_t* block = get_from_block_list();

            if (block->full() && !block_list.empty())
              {
              auto& index = block_index.template get<by_address>();
              index.erase(block->get_chunks_address());
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
        chunk_t* chunk = reinterpret_cast<chunk_t*>(object);
        const auto& index = block_index.template get<by_address>();
        const auto found = index.lower_bound(chunk);
        assert(found != index.cend());
        block_t* block = const_cast<block_t*>(&(*found));
        block->return_object_memory(chunk);
        if (block != current_block)
          add_to_block_list(block);
        }

      block_t* allocate_block()
        {
        auto insert_result = block_index.emplace();
        return const_cast<block_t*>(&(*insert_result.first));
        }

      void add_to_block_list(block_t* block)
        {
        if (!block->on_list)
          {
          block->on_list = true;
          block_list.push_front(block);
          ++block_list_size;
          }
        }

      block_t* get_from_block_list()
        {
        assert(!block_list.empty());
        block_t* block = block_list.front();
        block->on_list = false;
        block_list.pop_front();
        return block;
        }

    private:
      block_index_t             block_index;
      block_list_t              block_list;
      block_t*                  current_block = nullptr;
      uint32_t                  allocated_count = 0;
      uint32_t                  block_list_size = 0;
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
