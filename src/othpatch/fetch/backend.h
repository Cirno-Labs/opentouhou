#pragma once

// Finds the CA certificates of the platform for HTTPS.

// Holds the CA certificates: a file with every certificate, a directory
// with one file per certificate, or neither when the TLS library finds
// them itself.
typedef struct {
    const char *file;
    const char *dir;
} FETCH_CA;

// Returns the CA certificates of the system. The strings stay valid for
// the life of the program.
FETCH_CA FetchBackend_FindCa(void);
