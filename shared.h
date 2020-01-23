#pragma once
#define INITGUID
#include <guiddef.h>
#include <cguid.h>
// IIDFromString, StringFromIID
#include <objbase.h>
// MultiByteToWideChar
#include <winnls.h>

#ifdef __GNUC__
#define allow_unused __attribute__ ((unused))
#else
#define allow_unused
#endif

static const GUID DPSPGUID_DPRUN = { 0xb1ed2367, 0x609b, 0x4c5c, { 0x87, 0x55, 0xd2, 0xa2, 0x9b, 0xb9, 0xa5, 0x54 } };
static const GUID DPAID_SelfID = { 0x58b7d5df, 0xc38d, 0x4039, { 0x87, 0x49, 0x3e, 0x5b, 0x65, 0x1d, 0x9e, 0xa5 } };

#define DPRUN_VERSION 0
#define GUID_STR_LEN 39

/**
 * Parse a GUID string surrounded by {} to a GUID struct.
 */
static HRESULT allow_unused guid_parse(char* input, GUID* out_guid) {
  wchar_t str[39];
  MultiByteToWideChar(CP_ACP, 0, input, -1, str, 39);
  str[38] = L'\0';
  return IIDFromString(str, out_guid);
}

/**
 * Stringify a GUID struct to a string surrounded by {}.
 * out_str must be a pointer to a preallocated buffer of size GUID_STR_LEN.
 */
static HRESULT allow_unused guid_stringify(const GUID* guid, char* out_str) {
  wchar_t* str;
  HRESULT result = StringFromIID(guid, &str);
  if (FAILED(result)) {
    return result;
  }

  // lets hope this thing is big enough!
  for (int i = 0; i < 38; i++) {
    out_str[i] = (char) str[i];
  }
  out_str[38] = '\0';

  CoTaskMemFree(str);
  return 0; // DP_OK
}
