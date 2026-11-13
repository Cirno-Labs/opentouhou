// SPDX-License-Identifier: GPL-3.0-only

#include "othpatch/fetch/backend.h"

#include "core/io/file.h"

FETCH_CA FetchBackend_FindCa(void)
{
    if (File_Exists("/etc/ssl/cert.pem")) {
        return (FETCH_CA) { .file = "/etc/ssl/cert.pem" };
    }
    return (FETCH_CA) {};
}
