#pragma once

#include <boost/interprocess/managed_mapped_file.hpp>
#include <fc/log/logger.hpp>
#include <string>
#include <cstdlib>  // For std::system
#include <fstream>  // For direct file writing
#include <iostream>

#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#endif

namespace chainbase {

// Store original VM dirty page parameter values for restoration
struct vm_dirty_params {
    static long dirty_bytes;
    static long dirty_background_bytes;
    static long dirty_expire_centisecs;
    static long swappiness;
    static bool values_saved;
};

// Initialize static members
long vm_dirty_params::dirty_bytes = 0;
long vm_dirty_params::dirty_background_bytes = 0;
long vm_dirty_params::dirty_expire_centisecs = 0;
long vm_dirty_params::swappiness = 0;
bool vm_dirty_params::values_saved = false;

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
        
        // Set VM dirty page parameters to match the locked memory size
        // This reduces writebacks when memory is locked
        try {
            // Check for container environment paths
            const char* proc_path = "/proc/sys/vm/dirty_bytes";
            const char* host_proc_path = "/host-proc/sys/vm/dirty_bytes";
            
            // Check if we're in a container with mapped host proc
            bool use_host_proc = false;
            {
                std::ifstream test_file(host_proc_path);
                if (test_file.good()) {
                    use_host_proc = true;
                    ilog("Detected container with mapped host proc filesystem");
                }
                test_file.close();
            }
            
            // Choose the appropriate path prefix
            std::string path_prefix = use_host_proc ? "/host-proc/sys/vm/" : "/proc/sys/vm/";
            
            // Save original values if not already saved
            if (!vm_dirty_params::values_saved) {
                try {
                    std::string line;
                    
                    // Read dirty_bytes
                    {
                        std::ifstream dirty_file(path_prefix + "dirty_bytes");
                        if (dirty_file.good() && std::getline(dirty_file, line)) {
                            vm_dirty_params::dirty_bytes = std::stol(line);
                        }
                        dirty_file.close();
                    }
                    
                    // Read dirty_background_bytes
                    {
                        std::ifstream dirty_bg_file(path_prefix + "dirty_background_bytes");
                        if (dirty_bg_file.good() && std::getline(dirty_bg_file, line)) {
                            vm_dirty_params::dirty_background_bytes = std::stol(line);
                        }
                        dirty_bg_file.close();
                    }
                    
                    // Read dirty_expire_centisecs
                    {
                        std::ifstream expire_file(path_prefix + "dirty_expire_centisecs");
                        if (expire_file.good() && std::getline(expire_file, line)) {
                            vm_dirty_params::dirty_expire_centisecs = std::stol(line);
                        }
                        expire_file.close();
                    }
                    
                    // Read swappiness
                    {
                        std::ifstream swappiness_file(path_prefix + "swappiness");
                        if (swappiness_file.good() && std::getline(swappiness_file, line)) {
                            vm_dirty_params::swappiness = std::stol(line);
                        }
                        swappiness_file.close();
                    }
                    
                    vm_dirty_params::values_saved = true;
                    ilog("Saved original VM dirty page parameters: dirty_bytes=${db}, dirty_background_bytes=${dbg}, dirty_expire_centisecs=${dec}, swappiness=${swap}",
                        ("db", vm_dirty_params::dirty_bytes)
                        ("dbg", vm_dirty_params::dirty_background_bytes)
                        ("dec", vm_dirty_params::dirty_expire_centisecs)
                        ("swap", vm_dirty_params::swappiness));
                } catch (const std::exception& e) {
                    wlog("Failed to read original VM dirty page parameters: ${e}", ("e", e.what()));
                }
            }
            
            // Try direct file writing first (more container-friendly)
            bool success = true;
            
            // Set dirty_bytes to 10% higher than the memory mapped region size
            size_t dirty_bytes_value = aligned_size + (aligned_size / 10);  // Add 10%
            
            // Set dirty_background_bytes to exactly the memory mapped size
            // This will start background writeback when dirty pages reach the memory-mapped size
            {
                std::ofstream dirty_bg_file(path_prefix + "dirty_background_bytes");
                if (dirty_bg_file.good()) {
                    dirty_bg_file << aligned_size;
                    dirty_bg_file.close();
                } else {
                    success = false;
                }
            }
            
            // Set dirty_bytes to 10% higher than memory mapped size
            {
                std::ofstream dirty_file(path_prefix + "dirty_bytes");
                if (dirty_file.good()) {
                    dirty_file << dirty_bytes_value;
                    dirty_file.close();
                } else {
                    success = false;
                }
            }
            
            // Set dirty_expire_centisecs
            {
                std::ofstream expire_file(path_prefix + "dirty_expire_centisecs");
                if (expire_file.good()) {
                    expire_file << 300000;
                    expire_file.close();
                } else {
                    success = false;
                }
            }
            
            // Set vm.swappiness to 10 (reduce swapping)
            {
                std::ofstream swappiness_file(path_prefix + "swappiness");
                if (swappiness_file.good()) {
                    swappiness_file << 10;
                    swappiness_file.close();
                } else {
                    success = false;
                }
            }
            
            // Fall back to system() calls if direct file writing failed
            if (!success) {
                ilog("Direct file writing to proc failed, falling back to system commands");
                
                std::string cmd_prefix = use_host_proc ? "echo " : "echo ";
                std::string cmd_suffix = use_host_proc ? " > /host-proc/sys/vm/" : " > /proc/sys/vm/";
                
                // Set dirty_background_bytes to exactly the memory mapped size
                std::string set_dirty_bg_cmd = cmd_prefix + std::to_string(aligned_size) + 
                                              cmd_suffix + "dirty_background_bytes";
                
                // Set dirty_bytes to 10% higher than memory mapped size
                std::string set_dirty_cmd = cmd_prefix + std::to_string(dirty_bytes_value) + 
                                           cmd_suffix + "dirty_bytes";
                
                std::string set_expire_cmd = cmd_prefix + "300000" + 
                                            cmd_suffix + "dirty_expire_centisecs";
                
                // Set vm.swappiness to 10
                std::string set_swappiness_cmd = cmd_prefix + "10" + 
                                              cmd_suffix + "swappiness";
                
                int ret1 = std::system(set_dirty_bg_cmd.c_str());
                int ret2 = std::system(set_dirty_cmd.c_str());
                int ret3 = std::system(set_expire_cmd.c_str());
                int ret4 = std::system(set_swappiness_cmd.c_str());
                
                success = (ret1 == 0 && ret2 == 0 && ret3 == 0 && ret4 == 0);
            }
            
            if (success) {
                ilog("Set vm.dirty_background_bytes=${bg_size}, vm.dirty_bytes=${size} (10% above), vm.dirty_expire_centisecs=300000, vm.swappiness=10", 
                    ("bg_size", aligned_size)("size", dirty_bytes_value));
            } else {
                wlog("Failed to set VM dirty page parameters. Container may need --privileged or --cap-add=SYS_ADMIN");
            }
        } catch (const std::exception& e) {
            wlog("Exception while setting VM dirty page parameters: ${e}", ("e", e.what()));
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
        
        // Restore original VM dirty page parameters if we previously saved them
        if (vm_dirty_params::values_saved) {
            try {
                // Check for container environment paths
                const char* host_proc_path = "/host-proc/sys/vm/dirty_bytes";
                
                // Check if we're in a container with mapped host proc
                bool use_host_proc = false;
                {
                    std::ifstream test_file(host_proc_path);
                    if (test_file.good()) {
                        use_host_proc = true;
                    }
                    test_file.close();
                }
                
                // Choose the appropriate path prefix
                std::string path_prefix = use_host_proc ? "/host-proc/sys/vm/" : "/proc/sys/vm/";
                
                // Try direct file writing first
                bool success = true;
                
                // Restore dirty_expire_centisecs first (this is important for proper order)
                {
                    std::ofstream expire_file(path_prefix + "dirty_expire_centisecs");
                    if (expire_file.good()) {
                        expire_file << vm_dirty_params::dirty_expire_centisecs;
                        expire_file.close();
                    } else {
                        success = false;
                    }
                }
                
                // Restore dirty_background_bytes
                {
                    std::ofstream dirty_bg_file(path_prefix + "dirty_background_bytes");
                    if (dirty_bg_file.good()) {
                        dirty_bg_file << vm_dirty_params::dirty_background_bytes;
                        dirty_bg_file.close();
                    } else {
                        success = false;
                    }
                }
                
                // Restore dirty_bytes
                {
                    std::ofstream dirty_file(path_prefix + "dirty_bytes");
                    if (dirty_file.good()) {
                        dirty_file << vm_dirty_params::dirty_bytes;
                        dirty_file.close();
                    } else {
                        success = false;
                    }
                }
                
                // Restore swappiness
                {
                    std::ofstream swappiness_file(path_prefix + "swappiness");
                    if (swappiness_file.good()) {
                        swappiness_file << vm_dirty_params::swappiness;
                        swappiness_file.close();
                    } else {
                        success = false;
                    }
                }
                
                // Fall back to system() calls if direct file writing failed
                if (!success) {
                    std::string cmd_prefix = use_host_proc ? "echo " : "echo ";
                    std::string cmd_suffix = use_host_proc ? " > /host-proc/sys/vm/" : " > /proc/sys/vm/";
                    
                    // Must restore in this order to avoid errors
                    std::string set_expire_cmd = cmd_prefix + std::to_string(vm_dirty_params::dirty_expire_centisecs) + 
                                               cmd_suffix + "dirty_expire_centisecs";
                    std::string set_dirty_bg_cmd = cmd_prefix + std::to_string(vm_dirty_params::dirty_background_bytes) + 
                                                 cmd_suffix + "dirty_background_bytes";
                    std::string set_dirty_cmd = cmd_prefix + std::to_string(vm_dirty_params::dirty_bytes) + 
                                              cmd_suffix + "dirty_bytes";
                    std::string set_swappiness_cmd = cmd_prefix + std::to_string(vm_dirty_params::swappiness) + 
                                                   cmd_suffix + "swappiness";
                    
                    int ret1 = std::system(set_expire_cmd.c_str());
                    int ret2 = std::system(set_dirty_bg_cmd.c_str());
                    int ret3 = std::system(set_dirty_cmd.c_str());
                    int ret4 = std::system(set_swappiness_cmd.c_str());
                    
                    success = (ret1 == 0 && ret2 == 0 && ret3 == 0 && ret4 == 0);
                }
                
                if (success) {
                    ilog("Restored original VM dirty page parameters: dirty_bytes=${db}, dirty_background_bytes=${dbg}, dirty_expire_centisecs=${dec}, swappiness=${swap}",
                        ("db", vm_dirty_params::dirty_bytes)
                        ("dbg", vm_dirty_params::dirty_background_bytes)
                        ("dec", vm_dirty_params::dirty_expire_centisecs)
                        ("swap", vm_dirty_params::swappiness));
                    
                    // Reset the saved flag
                    vm_dirty_params::values_saved = false;
                } else {
                    wlog("Failed to restore original VM dirty page parameters");
                }
            } catch (const std::exception& e) {
                wlog("Exception while restoring VM dirty page parameters: ${e}", ("e", e.what()));
            }
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
