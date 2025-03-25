#pragma once

#include <chainbase/allocator_helpers.hpp>

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/core/demangle.hpp>

namespace chainbase {

  constexpr uint32_t DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE = 1 << 12; // 4k

#if defined(ENABLE_MULTI_INDEX_POOL_ALLOCATOR)
  static_assert((DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE & (DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE-1)) == 0,
    "DEFAULT_MULTI_INDEX_POOL_ALLOCATOR_BLOCK_SIZE should be power of 2!");
#endif

  constexpr uint32_t DEFAULT_UNDO_STATE_POOL_ALLOCATOR_BLOCK_SIZE = 1 << 12; // 4k

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

      //typedef std::false_type is_always_equal;

    public:
      template <bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(TSegmentManager* _segment_manager, std::enable_if_t<_USE_MANAGED_MAPPED_FILE>* = nullptr) noexcept
        : base_class_t(_segment_manager),
          block_allocator(_segment_manager),
          blocks(typename decltype(blocks)::allocator_type(_segment_manager))
        {
        }
      template <bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(std::enable_if_t<!_USE_MANAGED_MAPPED_FILE>* = nullptr) noexcept
        {
        }
      template <typename T2, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(const pool_allocator_t<T2, BLOCK_SIZE, TSegmentManager, USE_MANAGED_MAPPED_FILE>& other,
        std::enable_if_t<_USE_MANAGED_MAPPED_FILE>* = nullptr) noexcept
        : base_class_t(other.get_segment_manager()),
          block_allocator(other.get_segment_manager()),
          blocks(typename decltype(blocks)::allocator_type(other.get_segment_manager()))
        {
        }
      template <typename T2, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      pool_allocator_t(const pool_allocator_t<T2, BLOCK_SIZE, TSegmentManager, USE_MANAGED_MAPPED_FILE>& other,
        std::enable_if_t<!_USE_MANAGED_MAPPED_FILE>* = nullptr) noexcept
        {
        }

      ~pool_allocator_t()
        {
        for (auto& block : blocks)
          deallocate_block(block);
        }

      pointer allocate(size_type n = 1)
        {
        assert(n == 1);
        ++allocated_count;
        return get_object_memory();
        }

      void deallocate(pointer p, size_type n = 1)
        {
        assert(n == 1);
        --allocated_count;
        return_object_memory(p);
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
      auto get_generic_allocator(std::enable_if_t<_USE_MANAGED_MAPPED_FILE>* = nullptr) const noexcept
        {
        return allocator<T2>(get_segment_manager());
        }

      template <typename T2, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      auto get_generic_allocator(std::enable_if_t<!_USE_MANAGED_MAPPED_FILE>* = nullptr) const noexcept
        {
        return allocator<T2>();
        }

      template <typename T2, uint32_t BLOCK_SIZE2 = BLOCK_SIZE, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      auto get_pool_allocator(std::enable_if<_USE_MANAGED_MAPPED_FILE>* = nullptr) const noexcept
        {
        return pool_allocator_t<T2, BLOCK_SIZE2, TSegmentManager, USE_MANAGED_MAPPED_FILE>(get_segment_manager());
        }

      template <typename T2, uint32_t BLOCK_SIZE2 = BLOCK_SIZE, bool _USE_MANAGED_MAPPED_FILE = USE_MANAGED_MAPPED_FILE>
      auto get_pool_allocator(std::enable_if<!_USE_MANAGED_MAPPED_FILE>* = nullptr) const noexcept
        {
        return pool_allocator_t<T2, BLOCK_SIZE2, TSegmentManager, USE_MANAGED_MAPPED_FILE>();
        }

      uint32_t get_block_count() const noexcept { return blocks.size(); }

      uint32_t get_allocated_count() const noexcept { return allocated_count; }

      std::string get_allocation_summary() const
        {
        std::string result(get_allocator_name() + "allocation summary:");
        result += "\nblocks: ";
        result += std::to_string(blocks.size());
        result += "\nallocated: ";
        result += std::to_string(allocated_count);
        result += "\nfree_list: ";
        result += std::to_string(free_list_size);
        return result;
        }

    private:
      union chunk_t;
      using chunk_ptr_t = std::conditional_t<USE_MANAGED_MAPPED_FILE, bip::offset_ptr<chunk_t>, chunk_t*>;

      union alignas(std::max(alignof(T), alignof(chunk_ptr_t))) chunk_t
        {
        char        memory[sizeof(T)];
        chunk_ptr_t next;

        T* get_object_memory()
          {
          return reinterpret_cast<T*>(memory);
          }
        };

      struct block_t
        {
        chunk_t   memory[BLOCK_SIZE]; // allocated memory block
        uint32_t  first_free; // memory+first_free point to first free memory chunk to use
        };

      using block_ptr_t = std::conditional_t<USE_MANAGED_MAPPED_FILE, bip::offset_ptr<block_t>, block_t*>;

    private:
      pointer get_object_memory()
        {
        if (free_list_size != 0)
          {
          chunk_ptr_t result = free_list;
          free_list = free_list->next;
          --free_list_size;
          return *reinterpret_cast<pointer*>(&result);
          }

        if (current_block == nullptr)
          current_block = allocate_block();

        T* result = current_block->memory[current_block->first_free++].get_object_memory();

        if (current_block->first_free == BLOCK_SIZE)
          current_block = nullptr; // block has no more free memory space

        return pointer(result);
        }

      void return_object_memory(pointer object_memory)
        {
        chunk_ptr_t chunk = *reinterpret_cast<chunk_ptr_t*>(&object_memory);
        chunk->next = free_list;
        free_list = chunk;
        ++free_list_size;
        }

      block_t* allocate_block()
        {
        block_t* result = &(*block_allocator.allocate(1));
        result->first_free = 0; // avoid call ctor
        blocks.emplace_back(result);
        return result;
        }

      void deallocate_block(block_ptr_t block)
        {
        block_allocator.deallocate(block, 1);
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
        return std::string("pool_allocator_t<") + get_type_name() + ">";
        }

    private:
      allocator<block_t>    block_allocator;
      block_ptr_t           current_block = nullptr;
      t_vector<block_ptr_t> blocks;
      chunk_ptr_t           free_list = nullptr; // list of freed memory chunks for reusing
      uint32_t              free_list_size = 0;
      uint32_t              allocated_count = 0;
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
