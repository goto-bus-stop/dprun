#pragma once
#include <ole2.h>

#define CHECK(tag, result) if (result != DP_OK) { printf("%s failed: %s\n", tag, get_error_message(result)); return result; }

char* get_error_message(HRESULT hr);
