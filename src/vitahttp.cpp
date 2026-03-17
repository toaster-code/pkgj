#include "vitahttp.hpp"

#include <curl/curl.h>
#include <fmt/format.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#define PKGI_USER_AGENT "libhttp/3.65 (PS Vita)"

struct pkgi_http
{
    bool used;
    CURL* curl;
    std::vector<uint8_t> buffer;
    size_t buffer_pos;
    uint64_t content_length;
    long http_status;
    bool aborted;
};

namespace
{
static pkgi_http g_http[4];

static size_t pkgi_curl_write(
        char* data, size_t size, size_t nmemb, void* userdata)
{
    pkgi_http* http = static_cast<pkgi_http*>(userdata);
    if (http->aborted)
        return 0; // returning 0 causes CURLE_WRITE_ERROR, aborting transfer
    size_t total = size * nmemb;
    http->buffer.insert(http->buffer.end(), data, data + total);
    return total;
}
} // namespace

VitaHttp::~VitaHttp()
{
    if (_http)
    {
        LOG("http close");
        if (_http->curl)
        {
            curl_easy_cleanup(_http->curl);
            _http->curl = nullptr;
        }
        _http->buffer.clear();
        _http->buffer.shrink_to_fit();
        _http->used = false;
    }
}

void VitaHttp::start(const std::string& url, uint64_t offset)
{
    if (_http)
        throw HttpError("HTTP connection already started");

    LOG("http get");

    pkgi_http* http = nullptr;
    for (size_t i = 0; i < 4; i++)
    {
        if (!g_http[i].used)
        {
            http = g_http + i;
            break;
        }
    }

    if (!http)
        throw HttpError("internal error: too many simultaneous http requests");

    LOGF("starting http GET request for {}", url);

    http->curl = curl_easy_init();
    if (!http->curl)
        throw HttpError("curl_easy_init failed");

    http->buffer.clear();
    http->buffer_pos = 0;
    http->content_length = 0;
    http->http_status = 0;
    http->aborted = false;

    curl_easy_setopt(http->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(http->curl, CURLOPT_USERAGENT, PKGI_USER_AGENT);
    curl_easy_setopt(http->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(http->curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(http->curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(http->curl, CURLOPT_TIMEOUT, 0L);
    // The Vita does not have a full CA bundle; skip peer/host verification
    curl_easy_setopt(http->curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(http->curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(http->curl, CURLOPT_WRITEFUNCTION, pkgi_curl_write);
    curl_easy_setopt(http->curl, CURLOPT_WRITEDATA, http);

    if (offset > 0)
    {
        curl_easy_setopt(
                http->curl,
                CURLOPT_RESUME_FROM_LARGE,
                static_cast<curl_off_t>(offset));
    }

    CURLcode res = curl_easy_perform(http->curl);
    if (res != CURLE_OK)
    {
        std::string err = fmt::format(
                "sceHttpSendRequest failed: curl error {}: {}",
                static_cast<int>(res),
                curl_easy_strerror(res));
        curl_easy_cleanup(http->curl);
        http->curl = nullptr;
        throw HttpError(err);
    }

    curl_easy_getinfo(http->curl, CURLINFO_RESPONSE_CODE, &http->http_status);

    curl_off_t cl = -1;
    curl_easy_getinfo(http->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
    http->content_length =
            (cl >= 0) ? static_cast<uint64_t>(cl) : http->buffer.size();

    LOGF("http status = {}, content_length = {}",
         http->http_status,
         http->content_length);

    http->used = true;
    _http = http;
}

int64_t VitaHttp::read(uint8_t* buffer, uint64_t size)
{
    check_status();

    size_t available = _http->buffer.size() - _http->buffer_pos;
    if (available == 0)
        return 0; // EOF

    size_t to_copy = std::min(static_cast<size_t>(size), available);
    std::memcpy(buffer, _http->buffer.data() + _http->buffer_pos, to_copy);
    _http->buffer_pos += to_copy;
    return static_cast<int64_t>(to_copy);
}

void VitaHttp::abort()
{
    if (_http)
        _http->aborted = true;
}

int64_t VitaHttp::get_length()
{
    check_status();
    LOGF("http response length = {}", _http->content_length);
    return static_cast<int64_t>(_http->content_length);
}

int VitaHttp::get_status()
{
    if (!_http)
        return 0;
    return static_cast<int>(_http->http_status);
}

void VitaHttp::check_status()
{
    if (_status_checked)
        return;
    _status_checked = true;

    const auto status = get_status();

    LOGF("http status code = {}", status);

    if (status != 200 && status != 206)
        throw HttpError(fmt::format("bad http status: {}", status));
}

VitaHttp::operator bool() const
{
    return _http;
}
