#pragma once
#include <ole2.h>

/* #define USE_TIMED_DBGLOG */
#define CHECK(tag, result) if (result != DP_OK) { printf("%s failed: %s\n", tag, get_error_message(result)); return result; }

// Get a human readable error message for the given HRESULT.
char* get_error_message(HRESULT hr);
