#pragma once

#include "core/base/result.h"
#include "core/base/types.h"

// Fetches the contents of URLs into memory, several at a time.
//
// http:// and https:// URLs go through curl. A transfer follows redirects
// to http and https only, and fails when it cannot connect within 30
// seconds, receives less than one byte per second for 60 seconds, ends
// with an HTTP status of 400 or more, or grows past FETCH_MAX_SIZE. HTTPS
// checks the certificate of the server against the CA certificates that
// $SSL_CERT_FILE names, or else against the system's certificates.
//
// A URL below https://api.github.com/ also carries the token in
// $GITHUB_TOKEN, or else $GH_TOKEN, when one is set, so that the releases
// of a private repository can be read. No other host receives it.
//
// file:// URLs read a local file, so that the tests need no server. The
// host must be empty or "localhost", percent escapes are decoded, and a
// query or fragment after '?' or '#' is ignored.

// Sets how many transfers run at the same time.
#define FETCH_MAX_TRANSFERS 8

// Sets the largest contents that a fetch accepts.
#define FETCH_MAX_SIZE ((size_t)64 * 1024 * 1024)

// Holds one URL to fetch and, after Fetch_Run, its outcome.
typedef struct {
    const char *url;
    // Holds the Accept header to send, or nullptr for none.
    const char *accept;
    // Holds the contents. Mem_Free releases data. On failure, data is
    // nullptr and size is 0.
    uint8_t *data;
    size_t size;
    // Holds the outcome, which must be consumed, such as by Fetch_Release.
    RESULT result;
} FETCH;

// Prepares curl and finds the CA certificates. Fetch_Shutdown releases
// both.
RESULT Fetch_Init(void);
void Fetch_Shutdown(void);

// Fetches every URL of fetches, at most FETCH_MAX_TRANSFERS at a time, and
// sets the contents and the result of each.
void Fetch_Run(FETCH *fetches, int32_t count);

// Releases the contents of a fetch and consumes its result.
void Fetch_Release(FETCH *fetch);
