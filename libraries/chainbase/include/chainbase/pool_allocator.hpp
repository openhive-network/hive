#pragma once

#include <chainbase/allocator_helpers.hpp>

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/core/demangle.hpp>

namespace chainbase {

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
        : block_allocator(_segment_manager),
          blocks(typename decltype(blocks)::allocator_type(_segment_manager)),
          free_list(typename decltype(free_list)::allocator_type(_segment_manager))
        {
        }
      template <bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(std::enable_if_t<!_USE_MANAGED_MAPPED_FILE>* = nullptr)
        {
        }
      template <typename T2, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(const pool_allocator_t<T2, BLOCK_SIZE, TSegmentManager, USE_MANAGED_MAPPED_FILE>& other,
        std::enable_if_t<_USE_MANAGED_MAPPED_FILE>* = nullptr)
        : block_allocator(other.get_segment_manager()),
          blocks(typename decltype(blocks)::allocator_type(other.get_segment_manager())),
          free_list(typename decltype(free_list)::allocator_type(other.get_segment_manager()))
        {
        }
      template <typename T2, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(const pool_allocator_t<T2, BLOCK_SIZE, TSegmentManager, USE_MANAGED_MAPPED_FILE>& other,
        std::enable_if_t<!_USE_MANAGED_MAPPED_FILE>* = nullptr)
        {
        }

      ~pool_allocator_t()
        {
        for (auto& block : blocks)
          deallocate_block(block);
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

      segment_manager_t* get_segment_manager() const { return block_allocator.get_segment_manager(); }

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

      uint32_t get_block_count() const { return blocks.size(); }

      uint32_t get_allocated_count() const { return allocated_count; }

      std::string get_allocation_summary() const
        {
        std::string result(get_allocator_name() + "allocation summary:");
        result += "\nblocks: ";
        result += std::to_string(blocks.size());
        result += "\nallocated: ";
        result += std::to_string(allocated_count);
        result += "\nfree_list: ";
        //result += std::to_string(free_list_size);
        result += std::to_string(free_list.size());
        return result;
        }

    private:
      struct chunk_t
        {
        alignas(T) char object_memory[sizeof(T)];

        T* get_object_memory()
          {
          return reinterpret_cast<T*>(object_memory);
          }
        };

      struct block_t
        {
        chunk_t   memory[BLOCK_SIZE]; // allocated memory block
        uint32_t  first_free = 0; // memory+first_free point to first free memory chunk to use
        };

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
        return std::string("pool_allocator_t<") + get_type_name() + ">";
        }

      T* get_object_memory()
        {
        if (!free_list.empty())
          {
          //--free_list_size;
          //T* result = free_list.front();
          //free_list.pop_front();
          T* result = free_list.back();
          free_list.pop_back();
          return result;
          }

        if (current_block == nullptr)
          current_block = allocate_block();

        T* result = current_block->memory[current_block->first_free++].get_object_memory();
        if (current_block->first_free == BLOCK_SIZE)
          current_block = nullptr; // block has no more free memory space

        return result;
        }

      void return_object_memory(T* object_memory)
        {
        //++free_list_size;
        //free_list.push_front(object_memory);
        free_list.push_back(object_memory);
        }

      block_t* allocate_block()
        {
        block_t* result = &(*block_allocator.allocate(1));
        ::new (result) block_t();
        blocks.push_back(result);
        return result;
        }

      void deallocate_block(block_t* block)
        {
        block_allocator.deallocate(block, 1);
        }

    private:
      allocator<block_t>  block_allocator;
      block_t*            current_block = nullptr;
      t_vector<block_t*>  blocks;
      //std::vector<block_t*> blocks;
      t_vector<T*>        free_list; // list of freed memory chunks for reusing
      //std::vector<T*> free_list;
      uint32_t            allocated_count = 0;
      //uint32_t            free_list_size = 0;
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
