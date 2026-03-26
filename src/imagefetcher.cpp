#include "imagefetcher.hpp"

#include "curlhttp.hpp"
#include "db.hpp"
#include "file.hpp"
#include "pkgi.hpp"

#include <chrono>
#include <fmt/format.h>
#include <mutex>

namespace
{
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
            "00_09_000/container/{}/{}/19/{}/{}/image?w=248&h=248",
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
    std::lock_guard<Mutex> lock(_mutex);
    return _texture;
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
            std::lock_guard<Mutex> lock(_mutex);
            // Bug 3: skip expensive load if we are already being destroyed.
            if (_abort)
                return;
            _texture = vita2d_load_JPEG_file(_path.c_str());
            if (_texture)
                return;
        }

        if (_url.empty())
            return;

        {
            std::lock_guard<Mutex> lock(_mutex);
            if (_abort)
                return;
            _http = std::make_unique<CurlHttp>(&_abort);
        }

        std::vector<uint8_t> data;
        data.reserve(32 * 1024);
        _http->start(_url, 0);
        if (_http->get_status() == 404)
        {
            std::lock_guard<Mutex> lock(_mutex);
            _http = nullptr;
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
        vita2d_texture* tex = nullptr;
        {
            std::lock_guard<Mutex> lock(_mutex);
            _http = nullptr;
            if (!_abort && !too_large && !data.empty())
                tex = vita2d_load_JPEG_buffer(data.data(), data.size());
            _texture = tex;
        }
        if (tex)
        {
            const auto tmp_path = _path + ".tmp";
            auto image_file = pkgi_create(tmp_path);
            if (image_file)
            {
                pkgi_write(image_file, data.data(), data.size());
                pkgi_close(image_file);
                pkgi_rename(tmp_path, _path);
            }
        }
    }
    catch (const std::exception& e)
    {
        LOGF("Failed to fetch cover image: {}", e.what());
        std::lock_guard<Mutex> lock(_mutex);
        _http = nullptr;
    }
}
