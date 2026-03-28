#pragma once

#include <string>
#include <vector>

void pkgi_log_buffer_append(const char* line);
std::vector<std::string> pkgi_log_buffer_snapshot();