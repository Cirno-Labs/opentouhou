#include "othpatch/fetch.h"

#include "othpatch/fetch/backend.h"
#include "othpatch/fetch/file.h"

#include "core/base/memory.h"
#include "core/base/string.h"

#include "version.h"

#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FETCH *fetch;
    CURL *curl;
    size_t capacity;
    bool too_large;
    char error[CURL_ERROR_SIZE];
} M_TRANSFER;

static bool m_Ready;
static FETCH_CA m_Ca;

static size_t M_Write(
    const char *const data, const size_t size, const size_t count,
    void *const userdata)
{
    M_TRANSFER *const transfer = userdata;
    FETCH *const fetch = transfer->fetch;
    const size_t bytes = size * count;
    if (bytes > FETCH_MAX_SIZE - fetch->size) {
        transfer->too_large = true;
        return 0;
    }
    if (fetch->size + bytes > transfer->capacity) {
        size_t capacity = transfer->capacity > 0 ? transfer->capacity : 65536;
        while (capacity < fetch->size + bytes) {
            capacity *= 2;
        }
        fetch->data = Mem_Realloc(fetch->data, capacity);
        transfer->capacity = capacity;
    }
    memcpy(fetch->data + fetch->size, data, bytes);
    fetch->size += bytes;
    return bytes;
}

static void M_Fail(FETCH *const fetch, const RESULT result)
{
    Mem_Free(fetch->data);
    fetch->data = nullptr;
    fetch->size = 0;
    fetch->result = result;
}

static bool M_Start(CURLM *const multi, M_TRANSFER *const transfer)
{
    FETCH *const fetch = transfer->fetch;
    CURL *const curl = curl_easy_init();
    if (curl == nullptr) {
        M_Fail(
            fetch, FAIL("Cannot fetch %s: curl_easy_init failed", fetch->url));
        return false;
    }
    transfer->curl = curl;
    curl_easy_setopt(curl, CURLOPT_URL, fetch->url);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 8L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "othpatch/" OTHPATCH_VERSION);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, transfer->error);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, M_Write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, transfer);
    curl_easy_setopt(curl, CURLOPT_PRIVATE, transfer);
    if (m_Ca.file != nullptr) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, m_Ca.file);
    }
    if (m_Ca.dir != nullptr) {
        curl_easy_setopt(curl, CURLOPT_CAPATH, m_Ca.dir);
    }
    const CURLMcode code = curl_multi_add_handle(multi, curl);
    if (code != CURLM_OK) {
        curl_easy_cleanup(curl);
        transfer->curl = nullptr;
        M_Fail(
            fetch,
            FAIL("Cannot fetch %s: %s", fetch->url, curl_multi_strerror(code)));
        return false;
    }
    return true;
}

static void M_Finish(
    CURLM *const multi, M_TRANSFER *const transfer, const CURLcode code)
{
    FETCH *const fetch = transfer->fetch;
    if (transfer->too_large) {
        M_Fail(
            fetch,
            FAIL(
                "Cannot fetch %s: larger than %zu bytes", fetch->url,
                FETCH_MAX_SIZE));
    } else if (code != CURLE_OK) {
        const char *const reason = transfer->error[0] != '\0'
            ? transfer->error
            : curl_easy_strerror(code);
        M_Fail(fetch, FAIL("Cannot fetch %s: %s", fetch->url, reason));
    } else {
        if (fetch->data == nullptr) {
            fetch->data = Mem_Alloc(0);
        }
        fetch->result = OK;
    }
    curl_multi_remove_handle(multi, transfer->curl);
    curl_easy_cleanup(transfer->curl);
    transfer->curl = nullptr;
}

static bool M_IsHttp(const char *const url)
{
    return Str_StartsWith(url, "http://") || Str_StartsWith(url, "https://");
}

RESULT Fetch_Init(void)
{
    const CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
    FAIL_IF(
        code != CURLE_OK, "Cannot start curl: %s", curl_easy_strerror(code));
    m_Ready = true;
    const char *const cert_file = getenv("SSL_CERT_FILE");
    if (cert_file != nullptr && cert_file[0] != '\0') {
        m_Ca = (FETCH_CA) { .file = cert_file };
    } else {
        m_Ca = FetchBackend_FindCa();
    }
    return OK;
}

void Fetch_Shutdown(void)
{
    if (m_Ready) {
        curl_global_cleanup();
        m_Ready = false;
    }
}

void Fetch_Run(FETCH *const fetches, const int32_t count)
{
    M_TRANSFER transfers[FETCH_MAX_TRANSFERS] = {};
    CURLM *const multi = curl_multi_init();
    int32_t next = 0;
    int32_t active = 0;
    while (next < count || active > 0) {
        while (next < count && active < FETCH_MAX_TRANSFERS) {
            FETCH *const fetch = &fetches[next++];
            *fetch = (FETCH) { .url = fetch->url };
            if (Str_StartsWith(fetch->url, "file:")) {
                fetch->result =
                    FetchFile_Read(fetch->url, &fetch->data, &fetch->size);
                continue;
            }
            if (!M_IsHttp(fetch->url)) {
                fetch->result = FAIL(
                    "Cannot fetch %s: only http, https and file URLs work",
                    fetch->url);
                continue;
            }
            if (multi == nullptr) {
                fetch->result =
                    FAIL("Cannot fetch %s: curl_multi_init failed", fetch->url);
                continue;
            }
            M_TRANSFER *transfer = transfers;
            while (transfer->fetch != nullptr) {
                transfer++;
            }
            *transfer = (M_TRANSFER) { .fetch = fetch };
            if (M_Start(multi, transfer)) {
                active++;
            } else {
                transfer->fetch = nullptr;
            }
        }
        if (active == 0) {
            continue;
        }
        int running = 0;
        curl_multi_perform(multi, &running);
        int left = 0;
        CURLMsg *message;
        while ((message = curl_multi_info_read(multi, &left)) != nullptr) {
            if (message->msg != CURLMSG_DONE) {
                continue;
            }
            M_TRANSFER *transfer = nullptr;
            curl_easy_getinfo(
                message->easy_handle, CURLINFO_PRIVATE, &transfer);
            M_Finish(multi, transfer, message->data.result);
            transfer->fetch = nullptr;
            active--;
        }
        if (active > 0) {
            curl_multi_poll(multi, nullptr, 0, 1000, nullptr);
        }
    }
    if (multi != nullptr) {
        curl_multi_cleanup(multi);
    }
}

void Fetch_Release(FETCH *const fetch)
{
    Mem_Free(fetch->data);
    fetch->data = nullptr;
    fetch->size = 0;
    Result_Ignore(fetch->result);
    fetch->result = OK;
}
