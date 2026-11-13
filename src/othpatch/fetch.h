#pragma once

#include "core/base/result.h"
#include "core/base/types.h"
#include "core/patch/fetch.h"

// Fetches the contents of URLs into memory, several at a time, for
// core/patch and for the release checks.
//
// http:// and https:// URLs go through curl. A transfer follows redirects
// to http and https only, and fails when it cannot connect within 30
// seconds, receives less than one byte per second for 60 seconds, ends
// with an HTTP status of 400 or more, or grows past FETCH_MAX_SIZE. HTTPS
// checks the certificate of the server against the CA certificates that
// $SSL_CERT_FILE names, or else against the system's certificates.
//
// file:// URLs read a local file, so that the tests need no server. The
// host must be empty or "localhost", percent escapes are decoded, and a
// query or fragment after '?' or '#' is ignored.

// Sets how many transfers run at the same time.
#define FETCH_MAX_TRANSFERS 8

// Sets the largest contents that a fetch accepts.
#define FETCH_MAX_SIZE ((size_t)64 * 1024 * 1024)

// Prepares curl and finds the CA certificates. Fetch_Shutdown releases
// both.
RESULT Fetch_Init(void);
void Fetch_Shutdown(void);

// Fetches every URL of fetches, at most FETCH_MAX_TRANSFERS at a time, as
// a PATCH_FETCH_FN.
void Fetch_Run(
    PATCH_FETCH *fetches, int32_t count, const PATCH_FETCH_PROGRESS *progress);
