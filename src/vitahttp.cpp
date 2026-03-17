#include "vitahttp.hpp"

#include <curl/curl.h>
#include <fmt/format.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include <psp2/kernel/threadmgr.h>

#define PKGI_USER_AGENT "libhttp/3.65 (PS Vita)"

// Max bytes to buffer ahead of consumer. When the buffer grows beyond this
// the write callback returns CURL_WRITEFUNC_PAUSE so curl stops reading from
// the network socket until read() drains enough data.
static constexpr size_t PKGI_HTTP_BUFFER_MAX = 4 * 1024 * 1024; // 4 MB

struct pkgi_http
{
    bool used;
    CURLM* multi;
    CURL* curl;
    std::vector<uint8_t> buffer;
    size_t buffer_pos;
    uint64_t content_length;
    long http_status;
    bool transfer_done;
    bool aborted;
    CURLcode transfer_result;
};

namespace
{
static pkgi_http g_http[4];

static size_t pkgi_curl_write(
        char* data, size_t size, size_t nmemb, void* userdata)
{
    pkgi_http* http = static_cast<pkgi_http*>(userdata);
    if (http->aborted)
        return 0; // causes CURLE_WRITE_ERROR, kills transfer cleanly

    size_t total = size * nmemb;
    http->buffer.insert(http->buffer.end(), data, data + total);

    // Pause transfer when buffer is full; read() will resume it
    if (http->buffer.size() - http->buffer_pos >= PKGI_HTTP_BUFFER_MAX)
        return CURL_WRITEFUNC_PAUSE;

    return total;
}

static void pkgi_http_cleanup(pkgi_http* http)
{
    if (http->multi)
    {
        if (http->curl)
            curl_multi_remove_handle(http->multi, http->curl);
        curl_multi_cleanup(http->multi);
        http->multi = nullptr;
    }
    if (http->curl)
    {
        curl_easy_cleanup(http->curl);
        http->curl = nullptr;
    }
    http->buffer.clear();
    http->buffer.shrink_to_fit();
    http->used = false;
}

// Drive the multi handle until at least min_bytes are buffered, or the
// transfer completes. Returns false on curl-level error (fills errbuf).
static bool pump(pkgi_http* http, size_t min_bytes, std::string& errbuf)
{
    while (!http->transfer_done &&
           (http->buffer.size() - http->buffer_pos) < min_bytes)
    {
        int still_running = 0;
        CURLMcode mc = curl_multi_perform(http->multi, &still_running);
        if (mc != CURLM_OK)
        {
            errbuf = fmt::format(
                    "curl_multi_perform: {}", curl_multi_strerror(mc));
            return false;
        }

        // Collect finished messages
        int msgs_left = 0;
        while (CURLMsg* msg = curl_multi_info_read(http->multi, &msgs_left))
        {
            if (msg->msg == CURLMSG_DONE)
            {
                http->transfer_result = msg->data.result;
                http->transfer_done = true;
            }
        }

        if (!still_running)
        {
            http->transfer_done = true;
            break;
        }

        // Avoid cpu spin: sleep 1ms instead of curl_multi_wait
        // (curl_multi_wait uses select() on SceNet sockets which can crash)
        if (!http->transfer_done &&
            (http->buffer.size() - http->buffer_pos) < min_bytes)
        {
            sceKernelDelayThread(1000 /*1 ms*/);
            // Resume a paused transfer if buffer has been drained enough
            if ((http->buffer.size() - http->buffer_pos) < PKGI_HTTP_BUFFER_MAX)
                curl_easy_pause(http->curl, CURLPAUSE_CONT);
        }
    }
    return true;
}
} // namespace

VitaHttp::~VitaHttp()
{
    if (_http)
    {
        LOG("http close");
        pkgi_http_cleanup(_http);
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

    http->multi = curl_multi_init();
    if (!http->multi)
    {
        curl_easy_cleanup(http->curl);
        http->curl = nullptr;
        throw HttpError("curl_multi_init failed");
    }

    http->buffer.clear();
    http->buffer_pos = 0;
    http->content_length = 0;
    http->http_status = 0;
    http->transfer_done = false;
    http->aborted = false;
    http->transfer_result = CURLE_OK;

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

    curl_multi_add_handle(http->multi, http->curl);

    // Pump until we have at least 1 byte so that status/content-length are
    // available (response headers have been processed by then).
    std::string errbuf;
    if (!pump(http, 1, errbuf))
    {
        pkgi_http_cleanup(http);
        throw HttpError(errbuf);
    }

    // Check for curl-level transfer error (e.g. TLS failure, DNS error)
    if (http->transfer_done && http->transfer_result != CURLE_OK)
    {
        std::string err = fmt::format(
                "sceHttpSendRequest failed: curl error {}: {}",
                static_cast<int>(http->transfer_result),
                curl_easy_strerror(http->transfer_result));
        pkgi_http_cleanup(http);
        throw HttpError(err);
    }

    curl_easy_getinfo(http->curl, CURLINFO_RESPONSE_CODE, &http->http_status);

    curl_off_t cl = -1;
    curl_easy_getinfo(http->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
    http->content_length = (cl >= 0) ? static_cast<uint64_t>(cl) : 0;

    LOGF("http status = {}, content_length = {}",
         http->http_status,
         http->content_length);

    http->used = true;
    _http = http;
}

int64_t VitaHttp::read(uint8_t* buffer, uint64_t size)
{
    check_status();

    // Resume a paused transfer if the buffer has been drained enough
    if (!_http->transfer_done &&
        (_http->buffer.size() - _http->buffer_pos) < PKGI_HTTP_BUFFER_MAX)
    {
        curl_easy_pause(_http->curl, CURLPAUSE_CONT);
    }

    // Drive curl until we have data or the transfer is done
    std::string errbuf;
    if (!pump(_http, 1, errbuf))
        throw HttpError(errbuf);

    if (_http->transfer_done && _http->transfer_result != CURLE_OK &&
        _http->transfer_result != CURLE_WRITE_ERROR /* aborted */)
    {
        throw HttpError(fmt::format(
                "HTTP download error: curl {}: {}",
                static_cast<int>(_http->transfer_result),
                curl_easy_strerror(_http->transfer_result)));
    }

    size_t available = _http->buffer.size() - _http->buffer_pos;
    if (available == 0)
        return 0; // EOF

    size_t to_copy = std::min(static_cast<size_t>(size), available);
    std::memcpy(buffer, _http->buffer.data() + _http->buffer_pos, to_copy);
    _http->buffer_pos += to_copy;

    // Compact buffer periodically to avoid unbounded growth
    if (_http->buffer_pos > PKGI_HTTP_BUFFER_MAX)
    {
        _http->buffer.erase(
                _http->buffer.begin(),
                _http->buffer.begin() +
                        static_cast<ptrdiff_t>(_http->buffer_pos));
        _http->buffer_pos = 0;
    }

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

