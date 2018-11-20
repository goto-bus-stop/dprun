#include "../shared.h"
#include "../debug.h"
#include "dpsp.h"
#include <winreg.h>
#include <dplay.h>

#define REG_DPRUN "Software\\Microsoft\\DirectPlay\\Service Providers\\DPRun"

static char g_dprun_path[MAX_PATH] = {0};
static char* get_dprun_path() {
  GetModuleFileName(NULL, g_dprun_path, MAX_PATH);
  // dprun.exe → dprun.dll
  // TODO try to make directplay call into the exe instead…
  int end = strlen(g_dprun_path);
  memcpy(&g_dprun_path[end - 4], ".dll", 5);
  return g_dprun_path;
}

HRESULT dpsp_register() {
  char* dprun_path = get_dprun_path();
  char* dprun_desc = "DPRun Lobbying Proxy";
  char dprun_guid[39];
  wchar_t* dprun_guidw;
  StringFromIID(&DPSPGUID_DPRUN, &dprun_guidw);

  for (int i = 0; i < 38; i++) {
    // Lossy cast is fine here bc it only includes ascii chars
    dprun_guid[i] = (char) dprun_guidw[i];
  }
  dprun_guid[38] = '\0';
  CoTaskMemFree(dprun_guidw);

  HKEY dprun_key;
  LSTATUS result = RegCreateKeyEx(
      HKEY_LOCAL_MACHINE,
      REG_DPRUN,
      0,
      NULL,
      REG_OPTION_VOLATILE, // Keep this key in memory only
      KEY_WRITE | KEY_WOW64_32KEY,
      NULL,
      &dprun_key,
      NULL);

  if (result == ERROR_SUCCESS) result = RegSetValueEx(dprun_key, "DescriptionA", 0, REG_SZ, (void*)dprun_desc, strlen(dprun_desc) + 1);
  if (result == ERROR_SUCCESS) result = RegSetValueEx(dprun_key, "DescriptionW", 0, REG_SZ, (void*)dprun_desc, strlen(dprun_desc) + 1);
  if (result == ERROR_SUCCESS) result = RegSetValueEx(dprun_key, "Guid", 0, REG_SZ, (void*)dprun_guid, strlen(dprun_guid) + 1);
  if (result == ERROR_SUCCESS) result = RegSetValueEx(dprun_key, "Path", 0, REG_SZ, (void*)dprun_path, strlen(dprun_path) + 1);
  DWORD val = 0;
  if (result == ERROR_SUCCESS) result = RegSetValueEx(dprun_key, "dwReserved1", 0, REG_DWORD, (void*)&val, sizeof(val));
  if (result == ERROR_SUCCESS) result = RegSetValueEx(dprun_key, "dwReserved2", 0, REG_DWORD, (void*)&val, sizeof(val));
  if (result == ERROR_SUCCESS) result = RegSetValueEx(dprun_key, "Private", 0, REG_DWORD, (void*)&val, sizeof(val));

  if (FAILED(result)) {
    return HRESULT_FROM_WIN32(result);
  }

  return DP_OK;
}

HRESULT dpsp_unregister() {
  LSTATUS result = RegDeleteKeyEx(
      HKEY_LOCAL_MACHINE,
      REG_DPRUN,
      KEY_WOW64_32KEY,
      0);

  if (FAILED(result)) {
    return HRESULT_FROM_WIN32(result);
  }

  return DP_OK;
}
