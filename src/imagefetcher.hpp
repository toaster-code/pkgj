#pragma once

#include "config.hpp"
#include "http.hpp"
#include "thread.hpp"

#ifndef PKGI_SIMULATOR
#include <vita2d.h>
#else
// Forward-declare as an opaque pointer in simulator builds.
// In sdl_backend.cpp the type is reinterpreted as SDL_Texture*.
struct vita2d_texture;
#endif

#include <atomic>
#include <vector>

class ImageFetcher
{
public:
    static constexpr size_t MAX_SIZE_BYTES = 100 * 1024;

    enum class Status
    {
        Pending,
        Downloading,
        Ready,
        Error,
    };

    ImageFetcher(const Config* config, DbItem* item);
    ~ImageFetcher();

    // Must be called from the MAIN thread every frame.
    // Will create the vita2d texture from pending data on first call after
    // the background thread has finished downloading.
    vita2d_texture* get_texture();
    Status get_status();

private:
    Mutex _mutex;

    std::string _path;
    std::string _url;
    std::atomic<bool> _abort{false};
    std::unique_ptr<Http> _http;
    vita2d_texture* _texture{nullptr};
    Status _status{Status::Pending};

    // Pending data for main-thread texture creation.
    // The background thread always saves the download to disk first and then
    // sets _pending_jpeg_path so the main thread uses vita2d_load_JPEG_file.
    // Using vita2d_load_JPEG_buffer before vita2d_load_JPEG_file has been
    // called causes a hard system freeze on PS Vita (JPEG decoder not
    // initialised), so the buffer path is intentionally unused.
    std::string _pending_jpeg_path; // local file path ready to load
    bool        _upload_pending{false};

    Thread _thread;

    void do_request();
};
