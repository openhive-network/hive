#pragma once

#include <string>

namespace hive { namespace utilities {

size_t perform_read(int fd, char* buffer, size_t to_read, size_t offset, const std::string& description);
void perform_write(int fd, const char* buffer, size_t to_write, size_t offset, const std::string& description);

}} /// namespace hive::utilities
