// SPDX-License-Identifier: GPL-3.0-only

#include "othpatch/fetch/backend.h"

#include "core/base/utils.h"
#include "core/io/file.h"

static const char *const m_BundlePaths[] = {
    "/etc/ssl/certs/ca-certificates.crt",
    "/etc/pki/tls/certs/ca-bundle.crt",
    "/etc/ssl/ca-bundle.pem",
    "/etc/pki/tls/cacert.pem",
    "/etc/ssl/cert.pem",
};

FETCH_CA FetchBackend_FindCa(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_BundlePaths); i++) {
        if (File_Exists(m_BundlePaths[i])) {
            return (FETCH_CA) { .file = m_BundlePaths[i] };
        }
    }
    if (File_Exists("/etc/ssl/certs")) {
        return (FETCH_CA) { .dir = "/etc/ssl/certs" };
    }
    return (FETCH_CA) {};
}
