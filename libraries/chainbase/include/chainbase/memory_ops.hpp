#pragma once

#include <boost/interprocess/managed_mapped_file.hpp>
#include <fc/log/logger.hpp>
#include <string>

#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#endif

namespace chainbase {

/**
 * Utility class to lock a memory-mapped file in RAM to prevent swapping
 * Currently only implemented for Linux systems
 */
class memory_lock {
public:
    /**
     * Lock the memory region of a mapped file in RAM
     * 
     * @param segment The boost::interprocess::managed_mapped_file to lock
     * @return true if locking was successful, false otherwise
     */
    static bool lock_memory(const boost::interprocess::managed_mapped_file* segment) {
#ifdef __linux__
        if (!segment) {
            ilog("Cannot lock null memory segment");
            return false;
        }

        void* addr = segment->get_address();
        size_t size = segment->get_size();
        
        if (addr == nullptr || size == 0) {
            ilog("Invalid memory segment address or size");
            return false;
        }          ilog("Locking ${size} bytes of shared memory at address ${addr_str} in RAM",
            ("size", size)("addr_str", std::to_string((uintptr_t)addr)));
            
        long page_size = sysconf(_SC_PAGESIZE);
        // Align address to page boundary (mlock requires page-aligned addresses)
        void* aligned_addr = (void*)((uintptr_t)addr & ~(page_size - 1));
        // Add the offset to the size to account for alignment
        size_t aligned_size = size + ((uintptr_t)addr - (uintptr_t)aligned_addr);
        
        if (mlock(aligned_addr, aligned_size) != 0) {
            elog("Failed to lock memory: ${error}", ("error", strerror(errno)));
            return false;
        }
        
        ilog("Successfully locked ${size} bytes of shared memory at address ${addr_str} in RAM", 
            ("size", aligned_size)("addr_str", std::to_string((uintptr_t)aligned_addr)));
        return true;
#else
        ilog("Memory locking is only supported on Linux systems");
        return false;
#endif
    }

    /**
     * Unlock previously locked memory region
     * 
     * @param segment The boost::interprocess::managed_mapped_file to unlock
     * @return true if unlocking was successful, false otherwise
     */
    static bool unlock_memory(const boost::interprocess::managed_mapped_file* segment) {
#ifdef __linux__
        if (!segment) {
            ilog("Cannot unlock null memory segment");
            return false;
        }

        void* addr = segment->get_address();
        size_t size = segment->get_size();
        
        if (addr == nullptr || size == 0) {
            ilog("Invalid memory segment address or size");
            return false;
        }          ilog("Unlocking ${size} bytes of shared memory at address ${addr_str}",
            ("size", size)("addr_str", std::to_string((uintptr_t)addr)));
            
        long page_size = sysconf(_SC_PAGESIZE);
        // Align address to page boundary (munlock requires page-aligned addresses)
        void* aligned_addr = (void*)((uintptr_t)addr & ~(page_size - 1));
        // Add the offset to the size to account for alignment
        size_t aligned_size = size + ((uintptr_t)addr - (uintptr_t)aligned_addr);
        
        if (munlock(aligned_addr, aligned_size) != 0) {
            elog("Failed to unlock memory: ${error}", ("error", strerror(errno)));
            return false;
        }
        
        ilog("Successfully unlocked ${size} bytes of shared memory at address ${addr_str}", 
            ("size", aligned_size)("addr_str", std::to_string((uintptr_t)aligned_addr)));
        return true;
#else
        ilog("Memory unlocking is only supported on Linux systems");
        return false;
#endif
    }
};

} // namespace chainbase
