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
    static long dirty_ratio;
    static long dirty_background_ratio;
    static bool values_saved;
};

// Static member declarations (definitions moved to .cpp file)

/**
 * Utility class to lock a memory-mapped file in RAM to prevent swapping
 * Currently only implemented for Linux systems
 */
class memory_lock {
public:
    /**
     * Set VM dirty page parameters to optimize for memory locking
     * 
     * @param segment The boost::interprocess::managed_mapped_file to use for size calculation
     * @return true if setting parameters was successful, false otherwise
     */
    static bool set_vm_parameters(const boost::interprocess::managed_mapped_file* segment) {
#ifdef __linux__
        if (!segment) {
            ilog("Cannot set VM parameters for null memory segment");
            return false;
        }

        void* addr = segment->get_address();
        size_t size = segment->get_size();
        
        if (addr == nullptr || size == 0) {
            ilog("Invalid memory segment address or size");
            return false;
        }
            
        long page_size = sysconf(_SC_PAGESIZE);
        // Align address to page boundary
        void* aligned_addr = (void*)((uintptr_t)addr & ~(page_size - 1));
        // Add the offset to the size to account for alignment
        size_t aligned_size = size + ((uintptr_t)addr - (uintptr_t)aligned_addr);
        ilog("Setting VM parameters to decrease writebacks");
        // Set VM dirty page parameters to match the locked memory size
        // This reduces writebacks when memory is locked
        try {
            // Check for container environment path
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
                    
                    // Read dirty_ratio
                    {
                        std::ifstream dirty_ratio_file(path_prefix + "dirty_ratio");
                        if (dirty_ratio_file.good() && std::getline(dirty_ratio_file, line)) {
                            vm_dirty_params::dirty_ratio = std::stol(line);
                        }
                        dirty_ratio_file.close();
                    }
                    
                    // Read dirty_background_ratio
                    {
                        std::ifstream dirty_bg_ratio_file(path_prefix + "dirty_background_ratio");
                        if (dirty_bg_ratio_file.good() && std::getline(dirty_bg_ratio_file, line)) {
                            vm_dirty_params::dirty_background_ratio = std::stol(line);
                        }
                        dirty_bg_ratio_file.close();
                    }
                    
                    vm_dirty_params::values_saved = true;
                    ilog("Saved original VM dirty page parameters: dirty_bytes=${db}, dirty_background_bytes=${dbg}, dirty_expire_centisecs=${dec}, swappiness=${swap}, dirty_ratio=${dr}, dirty_background_ratio=${dbr}",
                        ("db", vm_dirty_params::dirty_bytes)
                        ("dbg", vm_dirty_params::dirty_background_bytes)
                        ("dec", vm_dirty_params::dirty_expire_centisecs)
                        ("swap", vm_dirty_params::swappiness)
                        ("dr", vm_dirty_params::dirty_ratio)
                        ("dbr", vm_dirty_params::dirty_background_ratio));
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
                
                // Try sysctl command first (works better in some container environments)
                std::string sysctl_cmd_prefix = "sysctl -w ";
                
                // If we're using host-proc, we need to modify our sysctl approach
                if (use_host_proc) {
                    // When in a container with host-proc mounted, we need to directly write to files
                    // since sysctl will still try to use the container's read-only /proc
                    
                    // Try direct writing to host-proc files
                    bool host_proc_success = true;
                    
                    // Set dirty_background_bytes
                    {
                        std::ofstream dirty_bg_file("/host-proc/sys/vm/dirty_background_bytes");
                        if (dirty_bg_file.good()) {
                            dirty_bg_file << aligned_size;
                            dirty_bg_file.close();
                        } else {
                            host_proc_success = false;
                            wlog("Failed to write to /host-proc/sys/vm/dirty_background_bytes");
                        }
                    }
                    
                    // Set dirty_bytes
                    {
                        std::ofstream dirty_file("/host-proc/sys/vm/dirty_bytes");
                        if (dirty_file.good()) {
                            dirty_file << dirty_bytes_value;
                            dirty_file.close();
                        } else {
                            host_proc_success = false;
                            wlog("Failed to write to /host-proc/sys/vm/dirty_bytes");
                        }
                    }
                    
                    // Set dirty_expire_centisecs
                    {
                        std::ofstream expire_file("/host-proc/sys/vm/dirty_expire_centisecs");
                        if (expire_file.good()) {
                            expire_file << 300000;
                            expire_file.close();
                        } else {
                            host_proc_success = false;
                            wlog("Failed to write to /host-proc/sys/vm/dirty_expire_centisecs");
                        }
                    }
                    
                    // Set vm.swappiness
                    {
                        std::ofstream swappiness_file("/host-proc/sys/vm/swappiness");
                        if (swappiness_file.good()) {
                            swappiness_file << 10;
                            swappiness_file.close();
                        } else {
                            host_proc_success = false;
                            wlog("Failed to write to /host-proc/sys/vm/swappiness");
                        }
                    }
                    
                    if (host_proc_success) {
                        ilog("Successfully set VM parameters using host-proc direct file writes");
                        success = true;
                    } else {
                        // As a last resort, try using echo with sudo
                        wlog("Direct writes to host-proc failed, trying echo commands");
                        
                        // Try using the 'cat' command with redirection or 'tee' which might have different permissions
                        // We'll try multiple approaches, working our way through fallbacks
                        
                        // First approach: Direct write with root - try other privileged user like 'root'
                        // Approach 1: Try running as root with su
                        std::string set_dirty_bg_cmd = std::string("su -c \"echo ") + std::to_string(aligned_size) + 
                                                     " > /host-proc/sys/vm/dirty_background_bytes\" root";
                        std::string set_dirty_cmd = std::string("su -c \"echo ") + std::to_string(dirty_bytes_value) + 
                                                  " > /host-proc/sys/vm/dirty_bytes\" root";
                        std::string set_expire_cmd = "su -c \"echo 300000 > /host-proc/sys/vm/dirty_expire_centisecs\" root";
                        std::string set_swappiness_cmd = "su -c \"echo 10 > /host-proc/sys/vm/swappiness\" root";
                        
                        int ret1 = std::system(set_dirty_bg_cmd.c_str());
                        int ret2 = std::system(set_dirty_cmd.c_str());
                        int ret3 = std::system(set_expire_cmd.c_str());
                        int ret4 = std::system(set_swappiness_cmd.c_str());
                        
                        // Count how many commands succeeded
                        int success_count = (ret1 == 0 ? 1 : 0) + (ret2 == 0 ? 1 : 0) + 
                                           (ret3 == 0 ? 1 : 0) + (ret4 == 0 ? 1 : 0);
                        
                        // If at least one command succeeded, consider it a partial success
                        success = (success_count > 0);
                        
                        // Approach 2: If su didn't work, try changing file permissions with chmod and write directly
                        if (!success) {
                            ilog("su approach failed, trying to use chmod to change file permissions");
                            
                            // Try to make the files writeable with chmod
                            std::string chmod_cmd_prefix = "chmod 666 " + (use_host_proc ? "/host-proc/sys/vm/" : "/proc/sys/vm/");
                            std::string chmod_dirty_bg = chmod_cmd_prefix + "dirty_background_bytes";
                            std::string chmod_dirty = chmod_cmd_prefix + "dirty_bytes";
                            std::string chmod_expire = chmod_cmd_prefix + "dirty_expire_centisecs";
                            std::string chmod_swappiness = chmod_cmd_prefix + "swappiness";
                            
                            // Try to chmod the files
                            std::system(chmod_dirty_bg.c_str());
                            std::system(chmod_dirty.c_str());
                            std::system(chmod_expire.c_str());
                            std::system(chmod_swappiness.c_str());
                            
                            // Now try direct writes again
                            {
                                std::ofstream dirty_bg_file(use_host_proc ? "/host-proc/sys/vm/dirty_background_bytes" : "/proc/sys/vm/dirty_background_bytes");
                                if (dirty_bg_file.good()) {
                                    dirty_bg_file << aligned_size;
                                    dirty_bg_file.close();
                                    success = true;
                                }
                            }
                            
                            {
                                std::ofstream dirty_file(use_host_proc ? "/host-proc/sys/vm/dirty_bytes" : "/proc/sys/vm/dirty_bytes");
                                if (dirty_file.good()) {
                                    dirty_file << dirty_bytes_value;
                                    dirty_file.close();
                                    success = true;
                                }
                            }
                            
                            {
                                std::ofstream expire_file(use_host_proc ? "/host-proc/sys/vm/dirty_expire_centisecs" : "/proc/sys/vm/dirty_expire_centisecs");
                                if (expire_file.good()) {
                                    expire_file << 300000;
                                    expire_file.close();
                                    success = true;
                                }
                            }
                            
                            {
                                std::ofstream swappiness_file(use_host_proc ? "/host-proc/sys/vm/swappiness" : "/proc/sys/vm/swappiness");
                                if (swappiness_file.good()) {
                                    swappiness_file << 10;
                                    swappiness_file.close();
                                    success = true;
                                }
                            }
                        }
                    }
                } else {
                    // Standard approach for non-container environments
                    std::string sysctl_dirty_bg_cmd = sysctl_cmd_prefix + "vm.dirty_background_bytes=" + std::to_string(aligned_size);
                    std::string sysctl_dirty_cmd = sysctl_cmd_prefix + "vm.dirty_bytes=" + std::to_string(dirty_bytes_value);
                    std::string sysctl_expire_cmd = sysctl_cmd_prefix + "vm.dirty_expire_centisecs=300000";
                    std::string sysctl_swappiness_cmd = sysctl_cmd_prefix + "vm.swappiness=10";
                    
                    int ret1 = std::system(sysctl_dirty_bg_cmd.c_str());
                    int ret2 = std::system(sysctl_dirty_cmd.c_str());
                    int ret3 = std::system(sysctl_expire_cmd.c_str());
                    int ret4 = std::system(sysctl_swappiness_cmd.c_str());
                    
                    // Check for actual success by parsing the output or checking return codes
                    // On some systems, sysctl might return 0 (success) even with "permission denied" 
                    // errors, so we need a better way to detect if the commands actually worked
                    
                    // Check if the values were actually changed by reading them back
                    bool sysctl_success = false;
                    try {
                        std::string line;
                        long dirty_bg_actual = -1;
                        long dirty_actual = -1;
                        long expire_actual = -1;
                        long swappiness_actual = -1;
                        
                        // Read dirty_background_bytes to verify change
                        {
                            std::ifstream dirty_bg_file(path_prefix + "dirty_background_bytes");
                            if (dirty_bg_file.good() && std::getline(dirty_bg_file, line)) {
                                dirty_bg_actual = std::stol(line);
                            }
                            dirty_bg_file.close();
                        }
                        
                        // Read dirty_bytes to verify change
                        {
                            std::ifstream dirty_file(path_prefix + "dirty_bytes");
                            if (dirty_file.good() && std::getline(dirty_file, line)) {
                                dirty_actual = std::stol(line);
                            }
                            dirty_file.close();
                        }
                        
                        // Read dirty_expire_centisecs to verify change
                        {
                            std::ifstream expire_file(path_prefix + "dirty_expire_centisecs");
                            if (expire_file.good() && std::getline(expire_file, line)) {
                                expire_actual = std::stol(line);
                            }
                            expire_file.close();
                        }
                        
                        // Read swappiness to verify change
                        {
                            std::ifstream swappiness_file(path_prefix + "swappiness");
                            if (swappiness_file.good() && std::getline(swappiness_file, line)) {
                                swappiness_actual = std::stol(line);
                            }
                            swappiness_file.close();
                        }
                        
                        // Check if at least some values match what we tried to set
                        // Allow for some tolerance in the values, as the system might round them
                        bool dirty_bg_match = (dirty_bg_actual == aligned_size || 
                                              (dirty_bg_actual > 0 && std::abs(dirty_bg_actual - (long)aligned_size) < (long)aligned_size * 0.05));
                        bool dirty_match = (dirty_actual == (long)dirty_bytes_value || 
                                          (dirty_actual > 0 && std::abs(dirty_actual - (long)dirty_bytes_value) < (long)dirty_bytes_value * 0.05));
                        bool expire_match = (expire_actual == 300000 || expire_actual > 200000);
                        bool swappiness_match = (swappiness_actual == 10 || swappiness_actual < 20);
                        
                        // If any of the important values were changed, consider it at least partial success
                        sysctl_success = (dirty_bg_match || dirty_match || expire_match || swappiness_match);
                        
                        if (sysctl_success) {
                            ilog("VM parameters verification: dirty_background_bytes=${bg_actual}/${bg_target}, dirty_bytes=${actual}/${target}, expire=${exp_actual}/${exp_target}, swappiness=${swap_actual}/${swap_target}",
                                ("bg_actual", dirty_bg_actual)("bg_target", aligned_size)
                                ("actual", dirty_actual)("target", dirty_bytes_value)
                                ("exp_actual", expire_actual)("exp_target", 300000)
                                ("swap_actual", swappiness_actual)("swap_target", 10));
                        } else {
                            // If any value is positive but doesn't match exactly, still consider it a success
                            if (dirty_bg_actual > 0 || dirty_actual > 0 || expire_actual > 0 || swappiness_actual >= 0) {
                                sysctl_success = true;
                                ilog("VM parameters were changed but don't match exactly what was requested. This is usually fine.");
                            }
                        }
                    } catch (const std::exception& e) {
                        wlog("Failed to verify VM parameter changes: ${e}", ("e", e.what()));
                        sysctl_success = false;
                    }
                    
                    if (sysctl_success) {
                        ilog("Successfully set VM parameters using sysctl command");
                        success = true;
                    } else {
                        // Check if sysctl commands returned success but didn't actually change values
                        if (ret1 == 0 && ret2 == 0 && ret3 == 0 && ret4 == 0) {
                            wlog("sysctl commands returned success but parameters were not actually changed. Check for 'permission denied' messages.");
                        } else {
                            wlog("sysctl commands failed with return codes: ${ret1}, ${ret2}, ${ret3}, ${ret4}", 
                                ("ret1", ret1)("ret2", ret2)("ret3", ret3)("ret4", ret4));
                        }
                        
                        // Fall back to echo commands if sysctl also failed
                        ilog("sysctl command failed, trying echo to proc files");
                        
                        std::string cmd_prefix = "echo ";
                        std::string cmd_suffix;
                        if (use_host_proc) {
                            cmd_suffix = " | sudo tee /host-proc/sys/vm/";
                        } else {
                            cmd_suffix = " | sudo tee /proc/sys/vm/";
                        }
                        
                        // Set dirty_background_bytes to exactly the memory mapped size
                        std::string set_dirty_bg_cmd = cmd_prefix + std::to_string(aligned_size) + 
                                                    cmd_suffix + "dirty_background_bytes > /dev/null";
                        
                        // Set dirty_bytes to 10% higher than memory mapped size
                        std::string set_dirty_cmd = cmd_prefix + std::to_string(dirty_bytes_value) + 
                                                  cmd_suffix + "dirty_bytes > /dev/null";
                        
                        std::string set_expire_cmd = cmd_prefix + "300000" + 
                                                    cmd_suffix + "dirty_expire_centisecs > /dev/null";
                        
                        // Set vm.swappiness to 10
                        std::string set_swappiness_cmd = cmd_prefix + "10" + 
                                                      cmd_suffix + "swappiness > /dev/null";

                        ret1 = std::system(set_dirty_bg_cmd.c_str());
                        ret2 = std::system(set_dirty_cmd.c_str());
                        ret3 = std::system(set_expire_cmd.c_str());
                        ret4 = std::system(set_swappiness_cmd.c_str());
                        
                        // Count how many commands succeeded
                        int success_count = (ret1 == 0 ? 1 : 0) + (ret2 == 0 ? 1 : 0) + 
                                           (ret3 == 0 ? 1 : 0) + (ret4 == 0 ? 1 : 0);
                        
                        // Consider it a success if at least one parameter was set
                        success = (success_count > 0);
                    }
                }
                
            }
            
            if (success) {
                ilog("Set vm.dirty_background_bytes=${bg_size}, vm.dirty_bytes=${size} (10% above), vm.dirty_expire_centisecs=300000, vm.swappiness=10", 
                    ("bg_size", aligned_size)("size", dirty_bytes_value));
                return true;
            } else {
                // As a last resort, try using multiple different approaches
                ilog("Previous VM parameter modification attempts failed, trying alternative approaches");
                
                // Approach 1: Try directly writing as root using su
                std::string cmd_prefix = "su -c \"echo ";
                std::string cmd_suffix = use_host_proc ? 
                    " > /host-proc/sys/vm/" : 
                    " > /proc/sys/vm/";
                cmd_suffix += "\" root";
                
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
                
                // Count how many commands succeeded
                int success_count = (ret1 == 0 ? 1 : 0) + (ret2 == 0 ? 1 : 0) + 
                                   (ret3 == 0 ? 1 : 0) + (ret4 == 0 ? 1 : 0);
                
                // If at least one command succeeded, consider it a partial success
                bool sudo_success = (success_count > 0);
                
                if (sudo_success) {
                    ilog("Successfully set at least some VM parameters using sudo");
                    return true;
                } else {
                    wlog("Failed to set VM dirty page parameters. Container may need --privileged or --cap-add=SYS_ADMIN");
                    return false;
                }
            }
        } catch (const std::exception& e) {
            wlog("Exception while setting VM dirty page parameters: ${e}", ("e", e.what()));
            return false;
        }
#else
        wlog("VM parameter modification is only supported on Linux systems");
        return false;
#endif
    }

    /**
     * Restore original VM dirty page parameters
     * 
     * @return true if restoring parameters was successful, false otherwise
     */
    static bool restore_vm_parameters() {
#ifdef __linux__
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
                bool at_least_one_success = false;
                
                // Restore dirty_expire_centisecs first (this is important for proper order)
                {
                    std::ofstream expire_file(path_prefix + "dirty_expire_centisecs");
                    if (expire_file.good()) {
                        expire_file << vm_dirty_params::dirty_expire_centisecs;
                        expire_file.close();
                        at_least_one_success = true;
                    } else {
                        success = false;
                        wlog("Failed to write to ${path}", ("path", path_prefix + "dirty_expire_centisecs"));
                    }
                }
                
                // Restore dirty_background_bytes
                {
                    std::ofstream dirty_bg_file(path_prefix + "dirty_background_bytes");
                    if (dirty_bg_file.good()) {
                        dirty_bg_file << vm_dirty_params::dirty_background_bytes;
                        dirty_bg_file.close();
                        at_least_one_success = true;
                    } else {
                        success = false;
                        wlog("Failed to write to ${path}", ("path", path_prefix + "dirty_background_bytes"));
                    }
                }
                
                // Restore dirty_bytes
                {
                    std::ofstream dirty_file(path_prefix + "dirty_bytes");
                    if (dirty_file.good()) {
                        dirty_file << vm_dirty_params::dirty_bytes;
                        dirty_file.close();
                        at_least_one_success = true;
                    } else {
                        success = false;
                        wlog("Failed to write to ${path}", ("path", path_prefix + "dirty_bytes"));
                    }
                }
                
                // Restore swappiness
                {
                    std::ofstream swappiness_file(path_prefix + "swappiness");
                    if (swappiness_file.good()) {
                        swappiness_file << vm_dirty_params::swappiness;
                        swappiness_file.close();
                        at_least_one_success = true;
                    } else {
                        success = false;
                        wlog("Failed to write to ${path}", ("path", path_prefix + "swappiness"));
                    }
                }
                
                // Restore dirty_ratio
                {
                    std::ofstream dirty_ratio_file(path_prefix + "dirty_ratio");
                    if (dirty_ratio_file.good()) {
                        dirty_ratio_file << vm_dirty_params::dirty_ratio;
                        dirty_ratio_file.close();
                        at_least_one_success = true;
                    } else {
                        success = false;
                        wlog("Failed to write to ${path}", ("path", path_prefix + "dirty_ratio"));
                    }
                }
                
                // Restore dirty_background_ratio
                {
                    std::ofstream dirty_bg_ratio_file(path_prefix + "dirty_background_ratio");
                    if (dirty_bg_ratio_file.good()) {
                        dirty_bg_ratio_file << vm_dirty_params::dirty_background_ratio;
                        dirty_bg_ratio_file.close();
                        at_least_one_success = true;
                    } else {
                        success = false;
                        wlog("Failed to write to ${path}", ("path", path_prefix + "dirty_background_ratio"));
                    }
                }
                
                // Use at_least_one_success as the indicator if any parameter was set
                success = at_least_one_success || success;
                
                // Fall back to system() calls if direct file writing failed
                if (!success) {
                    wlog("Direct file writing to restore VM parameters failed, falling back to system commands");
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
                    std::string set_dirty_ratio_cmd = cmd_prefix + std::to_string(vm_dirty_params::dirty_ratio) + 
                                                      cmd_suffix + "dirty_ratio";
                    std::string set_dirty_bg_ratio_cmd = cmd_prefix + std::to_string(vm_dirty_params::dirty_background_ratio) + 
                                                          cmd_suffix + "dirty_background_ratio";
                    
                    int ret1 = std::system(set_expire_cmd.c_str());
                    int ret2 = std::system(set_dirty_bg_cmd.c_str());
                    int ret3 = std::system(set_dirty_cmd.c_str());
                    int ret4 = std::system(set_swappiness_cmd.c_str());
                    int ret5 = std::system(set_dirty_ratio_cmd.c_str());
                    int ret6 = std::system(set_dirty_bg_ratio_cmd.c_str());
                    
                    // Count how many commands succeeded
                    int success_count = (ret1 == 0 ? 1 : 0) + (ret2 == 0 ? 1 : 0) + 
                                      (ret3 == 0 ? 1 : 0) + (ret4 == 0 ? 1 : 0) +
                                      (ret5 == 0 ? 1 : 0) + (ret6 == 0 ? 1 : 0);
                    
                    // If at least one command succeeded, consider it a partial success
                    success = (success_count > 0);
                    
                    // If echo commands failed, try with sudo as a last resort
                    if (!success) {
                        wlog("Echo commands to restore VM parameters failed, trying with root access");
                        std::string cmd_prefix = "su -c \"echo ";
                        std::string cmd_suffix = use_host_proc ? 
                            " > " + path_prefix : 
                            " > " + path_prefix;
                        cmd_suffix += "\" root";
                        
                        // Must restore in this order to avoid errors
                        set_expire_cmd = cmd_prefix + std::to_string(vm_dirty_params::dirty_expire_centisecs) + 
                                       cmd_suffix + "dirty_expire_centisecs";
                        set_dirty_bg_cmd = cmd_prefix + std::to_string(vm_dirty_params::dirty_background_bytes) + 
                                         cmd_suffix + "dirty_background_bytes";
                        set_dirty_cmd = cmd_prefix + std::to_string(vm_dirty_params::dirty_bytes) + 
                                      cmd_suffix + "dirty_bytes";
                        set_swappiness_cmd = cmd_prefix + std::to_string(vm_dirty_params::swappiness) + 
                                           cmd_suffix + "swappiness";
                        set_dirty_ratio_cmd = cmd_prefix + std::to_string(vm_dirty_params::dirty_ratio) + 
                                              cmd_suffix + "dirty_ratio";
                        set_dirty_bg_ratio_cmd = cmd_prefix + std::to_string(vm_dirty_params::dirty_background_ratio) + 
                                                  cmd_suffix + "dirty_background_ratio";
                        
                        ret1 = std::system(set_expire_cmd.c_str());
                        ret2 = std::system(set_dirty_bg_cmd.c_str());
                        ret3 = std::system(set_dirty_cmd.c_str());
                        ret4 = std::system(set_swappiness_cmd.c_str());
                        ret5 = std::system(set_dirty_ratio_cmd.c_str());
                        ret6 = std::system(set_dirty_bg_ratio_cmd.c_str());
                        
                        // Count how many commands succeeded
                        success_count = (ret1 == 0 ? 1 : 0) + (ret2 == 0 ? 1 : 0) + 
                                        (ret3 == 0 ? 1 : 0) + (ret4 == 0 ? 1 : 0) +
                                        (ret5 == 0 ? 1 : 0) + (ret6 == 0 ? 1 : 0);
                        
                        // If at least one command succeeded, consider it a partial success
                        success = (success_count > 0);
                    }
                }
                
                if (success) {
                    ilog("Restored original VM dirty page parameters: dirty_bytes=${db}, dirty_background_bytes=${dbg}, dirty_expire_centisecs=${dec}, swappiness=${swap}, dirty_ratio=${dr}, dirty_background_ratio=${dbr}",
                        ("db", vm_dirty_params::dirty_bytes)
                        ("dbg", vm_dirty_params::dirty_background_bytes)
                        ("dec", vm_dirty_params::dirty_expire_centisecs)
                        ("swap", vm_dirty_params::swappiness)
                        ("dr", vm_dirty_params::dirty_ratio)
                        ("dbr", vm_dirty_params::dirty_background_ratio));
                    
                    // Reset the saved flag
                    vm_dirty_params::values_saved = false;
                    return true;
                } else {
                    wlog("Failed to restore original VM dirty page parameters");
                    return false;
                }
            } catch (const std::exception& e) {
                wlog("Exception while restoring VM dirty page parameters: ${e}", ("e", e.what()));
                return false;
            }
        }
        return true; // If we never saved parameters, there's nothing to restore
#else
        wlog("VM parameter restoration is only supported on Linux systems");
        return false;
#endif
    }

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
        }          
        
        ilog("Locking ${size} bytes of shared memory at address ${addr_str} in RAM",
            ("size", size)("addr_str", std::to_string((uintptr_t)addr)));
            
        long page_size = sysconf(_SC_PAGESIZE);
        // Align address to page boundary (mlock requires page-aligned addresses)
        void* aligned_addr = (void*)((uintptr_t)addr & ~(page_size - 1));
        // Add the offset to the size to account for alignment
        size_t aligned_size = size + ((uintptr_t)addr - (uintptr_t)aligned_addr);
        
        // Try to set VM parameters but don't fail the whole operation if it doesn't work
        bool vm_params_success = set_vm_parameters(segment);
        if (!vm_params_success) {
            wlog("Failed to set VM parameters, but continuing with memory locking anyway");
            wlog("Performance may be affected, but functionality should remain intact");
            wlog("To fix VM parameter issues, ensure the container has proper privileges");
            wlog("or run with host network mode (--net=host) and --privileged flag");
        }
        
        // Now attempt to lock the memory
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
        }          
        
        ilog("Unlocking ${size} bytes of shared memory at address ${addr_str}",
            ("size", size)("addr_str", std::to_string((uintptr_t)addr)));
            
        long page_size = sysconf(_SC_PAGESIZE);
        // Align address to page boundary (munlock requires page-aligned addresses)
        void* aligned_addr = (void*)((uintptr_t)addr & ~(page_size - 1));
        // Add the offset to the size to account for alignment
        size_t aligned_size = size + ((uintptr_t)addr - (uintptr_t)aligned_addr);

        // Restore VM parameters first (even if memory unlocking fails)
        restore_vm_parameters();
        
        // Attempt to unlock the memory region
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
