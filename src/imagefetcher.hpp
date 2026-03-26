#pragma once

#include "config.hpp"
#include "http.hpp"
#include "thread.hpp"

#include <vita2d.h>

class ImageFetcher
{
public:
    static constexpr size_t MAX_SIZE_BYTES = 100 * 1024;

    ImageFetcher(const Config* config, DbItem* item);
    ~ImageFetcher();

    vita2d_texture* get_texture();

private:
    Mutex _mutex;

    std::string _path;
    std::string _url;
    bool _abort{false};
    std::unique_ptr<Http> _http;
    vita2d_texture* _texture{nullptr};

    Thread _thread;

    void do_request();
};
