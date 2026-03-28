#include "logbuffer.hpp"

#include <deque>
#include <mutex>

namespace
{
std::mutex g_log_mutex;
std::deque<std::string> g_log_lines;

constexpr std::size_t MaxLogLines = 512;
}

void pkgi_log_buffer_append(const char* line)
{
    if (!line)
        return;

    std::string text(line);
    while (!text.empty() &&
           (text.back() == '\n' || text.back() == '\r'))
    {
        text.pop_back();
    }

    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_log_lines.emplace_back(std::move(text));
    while (g_log_lines.size() > MaxLogLines)
        g_log_lines.pop_front();
}

std::vector<std::string> pkgi_log_buffer_snapshot()
{
    std::lock_guard<std::mutex> lock(g_log_mutex);
    return {g_log_lines.begin(), g_log_lines.end()};
}