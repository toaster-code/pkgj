#include "imagefetcher.hpp"

#include "db.hpp"
#include "file.hpp"
#include "pkgi.hpp"
#include "vitahttp.hpp"

#ifndef PKGI_SIMULATOR
#include <vita2d.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
extern SDL_Renderer* g_sdl_renderer;
static vita2d_texture* sim_load_jpeg_file(const char* path)
{
    SDL_Surface* s = IMG_Load(path);
    if (!s) return nullptr;
    SDL_Texture* t = SDL_CreateTextureFromSurface(g_sdl_renderer, s);
    SDL_FreeSurface(s);
    return reinterpret_cast<vita2d_texture*>(t);
}
#define vita2d_load_JPEG_file(p)     sim_load_jpeg_file(p)
#define vita2d_wait_rendering_done() ((void)0)
#define vita2d_free_texture(t)       SDL_DestroyTexture(reinterpret_cast<SDL_Texture*>(t))
#endif

#include <chrono>
#include <fmt/format.h>
#include <mutex>

namespace
{
bool uses_default_store_source(const Config* config)
{
    return !config || config->thumbnail_url.empty();
}

std::string get_store_image_url(DbItem* item)
{
    std::string country_abbv = "USA";
    std::string language = "en";
    switch (pkgi_get_region(item->titleid))
    {
    case RegionASA:
    {
        language = "zh";
        country_abbv = "HK";
        const std::string region = item->content.substr(0, 6);
        if (item->name.find("CHN") != std::string::npos)
        {
            country_abbv = "CN";
        }
        else if (region.compare("HP0507") == 0)
        {
            language = "ko";
            country_abbv = "KR";
        }
        else if (region.compare("HP2005"))
        {
            language = "en";
        }
    }
    break;
    case RegionJPN:
        country_abbv = "JP";
        language = "ja";
        break;
    case RegionEUR:
        country_abbv = "GB";
        break;
    default:
        country_abbv = "US";
    }
    return fmt::format(
            "https://store.playstation.com/store/api/chihiro/"
            "00_09_000/container/{}/{}/19/{}/{}/image?w=248",
            country_abbv,
            language,
            item->content,
            pkgi_time_msec());
}

std::string get_image_path(const Config* config, DbItem* item)
{
    const std::string folder = config && !config->thumbnail_folder.empty()
            ? config->thumbnail_folder
            : "ux0:pkgj/cover";

    if ((!config || config->thumbnail_folder.empty()) &&
        uses_default_store_source(config))
        return fmt::format("{}/{}.cover.jpg", folder, item->titleid);

    return fmt::format("{}/{}.jpg", folder, item->titleid);
}

std::string get_image_url(const Config* config, DbItem* item)
{
    if (config && !config->thumbnail_url.empty())
        return fmt::format("{}/{}.jpg", config->thumbnail_url, item->titleid);
    return get_store_image_url(item);
}

void ensure_image_folder(const Config* config)
{
    const std::string folder = config && !config->thumbnail_folder.empty()
            ? config->thumbnail_folder
            : "ux0:pkgj/cover";
    pkgi_mkdirs(folder.c_str());
}
}

ImageFetcher::ImageFetcher(const Config* config, DbItem* item)
    : _mutex("image_fetcher_mutex")
    , _path(get_image_path(config, item))
    , _url(get_image_url(config, item))
    , _thread("image_fetcher", [this] { do_request(); })
{
    ensure_image_folder(config);
}

ImageFetcher::~ImageFetcher()
{
    Http* http;
    {
        std::lock_guard<Mutex> lock(_mutex);
        _abort = true;
        http = _http.get();
    }
    if (http)
        http->abort();
    _thread.join();
    // Wait for any in-flight GPU frame to finish before releasing the texture.
    // vita2d queues draw commands asynchronously; the destructor can run right
    // after render() while the GPU is still reading this texture. Without the
    // wait, vita2d_free_texture triggers a GPU driver crash.
    // The stall is at most one frame (~16 ms) and only occurs when the user
    // closes the game view — not on every frame.
    if (_texture)
    {
        vita2d_wait_rendering_done();
        vita2d_free_texture(_texture);
    }
}

vita2d_texture* ImageFetcher::get_texture()
{
    // ── Fast path: texture already created ─────────────────────────────────
    {
        std::lock_guard<Mutex> lock(_mutex);
        if (!_upload_pending)
            return _texture;
    }

    // ── Take pending work under lock, then create texture OUTSIDE the lock ──
    // vita2d texture creation is only safe on the main (rendering) thread.
    bool        do_file = false;
    std::string path;

    {
        std::lock_guard<Mutex> lock(_mutex);
        if (!_upload_pending)
            return _texture; // another thread beat us (shouldn't happen)
        _upload_pending = false;
        if (!_pending_jpeg_path.empty())
        {
            do_file = true;
            path    = std::move(_pending_jpeg_path);
        }
    }

    vita2d_texture* tex = nullptr;
    if (do_file)
    {
        tex = vita2d_load_JPEG_file(path.c_str());
        if (!tex)
        {
            // Corrupt or invalid cached file — delete it so the next open
            // triggers a fresh download instead of looping on the same error.
            LOGFW("vita2d_load_JPEG_file failed for {}, removing corrupt cache",
                 path);
            pkgi_rm(path.c_str());
        }
    }

    {
        std::lock_guard<Mutex> lock(_mutex);
        _texture = tex;
        _status  = tex ? Status::Ready : Status::Error;
    }
    return tex;
}

ImageFetcher::Status ImageFetcher::get_status()
{
    std::lock_guard<Mutex> lock(_mutex);
    return _status;
}

void ImageFetcher::do_request()
{
    using namespace std::chrono;
    const auto start_time = steady_clock::now();
    const auto timeout = seconds(8);

    try
    {
        if (pkgi_file_exists(_path.c_str()))
        {
            {
                std::lock_guard<Mutex> lock(_mutex);
                if (_abort)
                    return;
                // Signal main thread to load the file; vita2d must run there.
                _pending_jpeg_path = _path;
                _upload_pending    = true;
                // Status stays Pending until get_texture() creates the texture.
            }
            return;
        }

        if (_url.empty())
        {
            std::lock_guard<Mutex> lock(_mutex);
            _status = Status::Error;
            return;
        }

        {
            std::lock_guard<Mutex> lock(_mutex);
            if (_abort)
                return;
            _status = Status::Downloading;
            _http = std::make_unique<VitaHttp>();
        }

        std::vector<uint8_t> data;
        data.reserve(32 * 1024);
        _http->start(_url, 0);
        if (_http->get_status() == 404)
        {
            std::lock_guard<Mutex> lock(_mutex);
            _http = nullptr;
            _status = Status::Error;
            return;
        }

        size_t pos = 0;
        bool too_large = false;
        while (true)
        {
            if (steady_clock::now() - start_time > timeout)
            {
                std::lock_guard<Mutex> lock(_mutex);
                _http = nullptr;
                _status = Status::Error;
                return;
            }

            {
                std::lock_guard<Mutex> lock(_mutex);
                if (_abort)
                {
                    _http = nullptr;
                    return;
                }
            }

            if (pos == data.size())
                data.resize(pos + 4096);

            const auto read = _http->read(data.data() + pos, data.size() - pos);
            if (read == 0)
                break;
            pos += read;

            if (pos > MAX_SIZE_BYTES)
            {
                too_large = true;
                break;
            }
        }

        data.resize(pos);
        {
            std::lock_guard<Mutex> lock(_mutex);
            _http = nullptr;
            if (_abort || too_large || data.empty())
            {
                _status = Status::Error;
                return;
            }
        }
        // Save JPEG to disk BEFORE signalling the main thread so that
        // vita2d_load_JPEG_file is used instead of vita2d_load_JPEG_buffer.
        // On PS Vita, vita2d_load_JPEG_buffer causes a hard system freeze
        // when called before vita2d_load_JPEG_file has ever been invoked
        // (the system JPEG decoder is not yet initialised).  Always going
        // through the on-disk path avoids this entirely.
        bool saved = false;
        void* image_file = nullptr;
        try
        {
            const auto tmp_path = _path + ".tmp";
            image_file = pkgi_create(tmp_path);
            pkgi_write(image_file, data.data(), data.size());
            pkgi_close(image_file);
            image_file = nullptr;
            pkgi_rename(tmp_path, _path);
            saved = true;
        }
        catch (const std::exception& e)
        {
            if (image_file)
                pkgi_close(image_file);
            LOGFW("Failed to save cover image to {}: {}", _path, e.what());
        }
        {
            std::lock_guard<Mutex> lock(_mutex);
            if (_abort)
                return;
            if (saved)
            {
                _pending_jpeg_path = _path;
                _upload_pending    = true;
            }
            else
            {
                _status = Status::Error;
            }
        }
    }
    catch (const std::exception& e)
    {
        LOGFW("Failed to fetch cover image: {}", e.what());
        std::lock_guard<Mutex> lock(_mutex);
        _http = nullptr;
        _status = Status::Error;
    }
}
