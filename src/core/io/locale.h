#pragma once

// Finds the language that the user prefers, as the system settings give it.

// Sets the size of the code buffers of a LOCALE, NUL included.
#define LOCALE_CODE_SIZE 4

// Holds a locale. language is an ISO 639 code of two or three lowercase
// letters, such as "pt". country is an ISO 3166 code of two uppercase
// letters or three digits, such as "BR", or an empty string when the locale
// names no country. An empty language means that the locale is unknown.
typedef struct {
    char language[LOCALE_CODE_SIZE];
    char country[LOCALE_CODE_SIZE];
} LOCALE;

// Returns the locale that the user prefers most. A locale that the system
// does not give, or gives in another form, is returned as unknown.
LOCALE Locale_GetPreferred(void);
