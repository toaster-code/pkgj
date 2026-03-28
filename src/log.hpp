#pragma once

#include <fmt/format.h>
#include <stdexcept>

#define LOG(msg, ...)                 \
    do                                \
    {                                 \
        pkgi_log(msg, ##__VA_ARGS__); \
    } while (0)
#define LOGF(msg, ...)                                     \
    do                                                     \
    {                                                      \
        pkgi_log(fmt::format(msg, ##__VA_ARGS__).c_str()); \
    } while (0)

template <typename E = std::runtime_error, typename... Args>
[[nodiscard]] E formatEx(Args&&... args)
{
    return E(fmt::format(std::forward<Args>(args)...));
}

void pkgi_log(const char* msg, ...);
